/*
  Color Sorting Machine v6 - NodeMCU ESP32 Carrier Board V1
  Fixed chamber positions measured from a HOME position, tray returns home after every ball.

  Pins (from the Robot Carrier Board manual)
    Servo      : IO5   (SERVO_M connector)
    Motor-1    : IN1=27, IN2=26, ENA=14 (L298N)
    TCS34725   : I2C SDA=21, SCL=22 (TCS34725 header)
    Buzzer module (low-level trigger, MH-FMD type):
                 VCC -> 3V3 pin on the ESP32 header, GND -> GND, I/O -> IO16 on the ESP32 header

  Home = the RED chamber under the chute. Turn the tray by hand to this position before power-on.
  Each colour moves the tray a fixed number of quarter-turns from home (see MOVE_STEPS).
  Positive = forward, negative = reverse.

  Serial (115200):
    r g y b = calibrate: put that colour ball in front of the sensor, then send the letter
    s = live reading, p = print saved calibration
    f = tray one step forward, v = tray one step reverse (use these to tune STEP_FWD_MS / STEP_REV_MS)
*/

#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// ---------- pins ----------
#define SERVO_PIN 5
#define IN1 27
#define IN2 26
#define ENA 14
#define BUZZER_PIN 16          // buzzer module I/O pin, straight from the ESP32 header

// ---------- tray movement (tune these) ----------
#define MOTOR_PWM 170          // 0-255
#define KICK_MS 40             // full-power kick at the start of each move, to overcome friction
#define STEP_FWD_MS 700        // run time for one quarter-turn forward
#define STEP_REV_MS 700        // run time for one quarter-turn in reverse
#define RETURN_HOME 1          // 1 = tray goes back to home after each ball

// tray steps from home for each colour: RED, GREEN, YELLOW, BLUE
// +1 = one chamber forward, +2 = two chambers forward, -1 = one chamber reverse
const int MOVE_STEPS[4] = {0, -1, 2, 1};

// ---------- other settings ----------
#define BENCH_TEST 0           // 1 = sensor + servo only (motor disabled)
#define DEBUG_PRINT 1          // print clear / threshold once per second
#define GATE_CLOSED 100        // servo angles - swap these two if your lid moves the wrong way
#define GATE_OPEN 10
#define GATE_OPEN_MS 800
#define SETTLE_MS 600          // wait for the ball to stop before reading
#define DETECT_FACTOR 1.25f    // ball detected when clear > baseline * this + DETECT_OFFSET
#define DETECT_OFFSET 20
#define MAX_MATCH_DIST 0.12f   // larger distance than this = UNKNOWN colour
#define RECHECK_TRIES 2        // extra colour readings before giving up and sounding the buzzer
#define BUZZER_ENABLED 1       // 0 = buzzer completely off

enum Color { RED = 0, GREEN = 1, YELLOW = 2, BLUE = 3, UNKNOWN = -1 };
const char* NAMES[4] = {"RED", "GREEN", "YELLOW", "BLUE"};

Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Servo gate;
ESP32PWM motorPwm;          // allocated through ESP32Servo so it can't clash with the servo
bool motorReady = false;
Preferences prefs;

// chromaticity centroids (r/c, g/c, b/c) - rough defaults, calibrate with your balls
float cal[4][3] = {
  {0.55f, 0.25f, 0.20f},  // RED
  {0.25f, 0.50f, 0.25f},  // GREEN
  {0.45f, 0.40f, 0.15f},  // YELLOW
  {0.15f, 0.30f, 0.55f}   // BLUE
};

float baselineClear = 0;    // clear-channel level with an empty basket
unsigned long lastDebug = 0;

// ---------- motor ----------
void runMotor(bool forward, unsigned long ms) {
  if (!motorReady) { Serial.println("Motor NOT ready (BENCH_TEST is 1?)"); return; }
  digitalWrite(IN1, forward ? HIGH : LOW);
  digitalWrite(IN2, forward ? LOW : HIGH);
  motorPwm.write(255);                       // kick
  delay(KICK_MS);
  motorPwm.write(MOTOR_PWM);
  if (ms > KICK_MS) delay(ms - KICK_MS);
  motorPwm.write(0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  delay(250);                                // let the tray stop
}

// move the tray by a number of quarter-turns (negative = reverse)
void moveTray(int steps) {
  if (steps == 0) return;
  bool forward = steps > 0;
  int n = abs(steps);
  unsigned long ms = (unsigned long)n * (forward ? STEP_FWD_MS : STEP_REV_MS);
  Serial.printf("Tray %s %d step(s) (%lu ms)\n", forward ? "forward" : "reverse", n, ms);
  runMotor(forward, ms);
}

// ---------- buzzer ----------
// Low-level-trigger module powered from 3.3V: pin HIGH = silent, pin LOW = beep.
void buzzerOff() {
#if BUZZER_ENABLED
  digitalWrite(BUZZER_PIN, HIGH);
#endif
}

void beep(int ms) {
#if BUZZER_ENABLED
  digitalWrite(BUZZER_PIN, LOW);
  delay(ms);
  buzzerOff();
#else
  delay(ms);
#endif
}

void warnBuzzer() {                          // error sound: three beeps
  for (int i = 0; i < 3; i++) { beep(250); delay(150); }
}

// ---------- gate ----------
void openGate()  { Serial.println("Gate OPEN");   gate.write(GATE_OPEN); }
void closeGate() { Serial.println("Gate CLOSED"); gate.write(GATE_CLOSED); }

// ---------- colour ----------
bool readSample(float &r, float &g, float &b, float &clear, int n = 5) {
  float sr = 0, sg = 0, sb = 0, sc = 0;
  for (int i = 0; i < n; i++) {
    uint16_t R, G, B, C;
    delay(60);
    tcs.getRawData(&R, &G, &B, &C);
    sr += R; sg += G; sb += B; sc += C;
  }
  clear = sc / n;
  if (clear < 1) return false;
  r = sr / sc; g = sg / sc; b = sb / sc;   // chromaticity
  return true;
}

float detectThreshold() { return baselineClear * DETECT_FACTOR + DETECT_OFFSET; }

bool ballPresent() {
  float r, g, b, c;
  if (!readSample(r, g, b, c, 2)) return false;
#if DEBUG_PRINT
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();
    Serial.printf("clear=%.0f  threshold=%.0f\n", c, detectThreshold());
  }
#endif
  return c > detectThreshold();
}

int classify() {
  float r, g, b, c;
  if (!readSample(r, g, b, c, 8)) return UNKNOWN;
  int best = UNKNOWN;
  float bestD = 9;
  for (int i = 0; i < 4; i++) {
    float dr = r - cal[i][0], dg = g - cal[i][1], db = b - cal[i][2];
    float d = sqrtf(dr * dr + dg * dg + db * db);
    if (d < bestD) { bestD = d; best = i; }
  }
  Serial.printf("r=%.2f g=%.2f b=%.2f clear=%.0f -> %s (dist %.3f)\n",
                r, g, b, c, best >= 0 ? NAMES[best] : "?", bestD);
  return (bestD <= MAX_MATCH_DIST) ? best : UNKNOWN;
}

void saveCal() { prefs.putBytes("cal", cal, sizeof(cal)); }

void calibrate(int idx) {
  float r, g, b, c;
  if (readSample(r, g, b, c, 15)) {
    cal[idx][0] = r; cal[idx][1] = g; cal[idx][2] = b;
    saveCal();
    Serial.printf("Saved %s: %.3f %.3f %.3f\n", NAMES[idx], r, g, b);
  }
}

void handleSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'r') calibrate(RED);
    else if (ch == 'g') calibrate(GREEN);
    else if (ch == 'y') calibrate(YELLOW);
    else if (ch == 'b') calibrate(BLUE);
    else if (ch == 'f') moveTray(1);
    else if (ch == 'v') moveTray(-1);
    else if (ch == 's') {
      float r, g, b, c;
      if (readSample(r, g, b, c, 5))
        Serial.printf("live r=%.3f g=%.3f b=%.3f clear=%.0f (baseline %.0f, threshold %.0f)\n",
                      r, g, b, c, baselineClear, detectThreshold());
    } else if (ch == 'p') {
      for (int i = 0; i < 4; i++)
        Serial.printf("%s: %.3f %.3f %.3f\n", NAMES[i], cal[i][0], cal[i][1], cal[i][2]);
    }
  }
}

// colour not recognised: beep until the ball is taken out of the basket
void waitForBallRemoved() {
  unsigned long lastBeep = 0;
  int clearCount = 0;
  while (clearCount < 3) {
    handleSerial();                          // calibration still works while waiting
    if (millis() - lastBeep > 2000) { warnBuzzer(); lastBeep = millis(); }
    if (!ballPresent()) clearCount++; else clearCount = 0;
    delay(200);
  }
  Serial.println("Ball removed - ready.");
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
#if BUZZER_ENABLED
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();                               // silent from the start
#endif
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // servo first, so it always gets its own 50 Hz timer
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  gate.setPeriodHertz(50);
  gate.attach(SERVO_PIN, 500, 2400);
  gate.write(GATE_CLOSED);

#if !BENCH_TEST
  motorPwm.attachPin(ENA, 1000, 8);
  motorPwm.write(0);
  motorReady = true;
#endif

  Wire.begin(21, 22);
  if (!tcs.begin()) {
    Serial.println("TCS34725 not found - check wiring and TCS_SW DIP switch");
    while (true) delay(1000);
  }

  prefs.begin("sorter", false);
  if (prefs.getBytesLength("cal") == sizeof(cal)) prefs.getBytes("cal", cal, sizeof(cal));

  // empty-basket reference (gate closed, basket empty)
  float r, g, b;
  delay(500);
  readSample(r, g, b, baselineClear, 10);
  Serial.printf("Ready. Empty-basket clear level: %.0f (detect above %.0f)\n",
                baselineClear, detectThreshold());
  Serial.println("Home position: RED chamber under the chute.");
}

void loop() {
  handleSerial();

  if (!ballPresent()) { delay(100); return; }

  delay(SETTLE_MS);                 // let the ball stop rolling
  int color = classify();
  for (int i = 0; i < RECHECK_TRIES && color == UNKNOWN; i++) {
    delay(300);
    color = classify();
  }

  if (color == UNKNOWN) {
    // error: buzzer sounds, lid stays closed, tray stays at home
    Serial.println("Colour NOT recognised - buzzer on, lid stays closed. Remove the ball.");
    waitForBallRemoved();
    return;
  }

  int steps = MOVE_STEPS[color];
  Serial.printf("Sorting to %s (%+d steps from home)\n", NAMES[color], steps);
  moveTray(steps);

  openGate();
  delay(GATE_OPEN_MS);
  closeGate();
  delay(500);

#if RETURN_HOME
  moveTray(-steps);                 // back to home (RED chamber)
#endif
  delay(300);
}

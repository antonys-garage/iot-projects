// TFT Display Pin Labeled  Connects to ESP32 Pin
// VCC                      3.3V,Main power supply
// GND                      GND,Ground
// CS                       IO22,Chip Select (SPI)
// RESET                    IO21,Screen Reset
// A0 (or DC/RS)            IO17,Data / Command Selection
// SDA (or MOSI)            IO23,SPI Master Out Slave In
// SCK (or CLK)             IO18,SPI Clock
// LED (or BL)              IO19,Backlight Control (High = On)
// BUZZER (optional)        IO4,Proximity buzzer - leave unwired if not used

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <ESP32Servo.h>      // Servo library compatible with ESP32
#include <SPI.h>

// --- PIN DEFINITIONS ---
#define TFT_CS    22
#define TFT_RST   21
#define TFT_DC    17  // Labeled A0 on your board
#define TFT_BL    19  // Backlight pin

#define TRIG_PIN  15
#define ECHO_PIN  13
#define SERVO_PIN 5
#define BUZZER_PIN 4   // optional - proximity beeper, safe to leave unconnected

// --- DISPLAY / CANVAS ---
// Everything draws to an off-screen canvas in RAM, then gets blasted to the
// TFT in one SPI burst per frame. Flicker-free, and lets us layer effects
// (trails, pings, fades) cheaply without touching the real display until
// the frame is fully composed.
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(160, 128);

Servo radarServo;

// --- RADAR GEOMETRY ---
const int centerX   = 80;
const int centerY   = 112;
const int radius    = 68;
const int maxDistance = 40;  // cm

// --- COLOR THEME ---
#define COL_BG        0x0000
#define COL_GRID      0x0320
#define COL_GRID_HI   0x2565
#define COL_SWEEP     0x07E0
#define COL_TEXT      0x87F0
#define COL_TEXT_DIM  0x2965
#define COL_NEAR      0xF800
#define COL_MID       0xFDA0
#define COL_FAR       0x07E0
#define COL_LOCK      0xFFFF

// --- SWEEP TRAIL ---
const int MAX_TRAIL = 6;
int trailAngles[MAX_TRAIL];
int trailCount = 0;

// --- BLIP HISTORY (steady detections that persist briefly then fade) ---
struct Blip { int x, y; uint8_t age; uint16_t color; bool active; };
const int MAX_BLIPS = 10;
const uint8_t BLIP_MAX_AGE = 18;
Blip blips[MAX_BLIPS];

// --- PING ANIMATION (one-shot expanding ring when a NEW object appears) ---
struct Ping { int x, y; uint8_t age; bool active; };
const int MAX_PINGS = 4;
const uint8_t PING_MAX_AGE = 10;
Ping pings[MAX_PINGS];
bool wasDetectedLastFrame = false;

// --- NEAREST-OBJECT TRACKING (per half-sweep pass) ---
long nearestDistancePass = -1;
int nearestX = 0, nearestY = 0;

int currentAngle = 15;
long currentDistance = -1;

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  radarServo.setPeriodHertz(50);
  radarServo.attach(SERVO_PIN, 500, 2400);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);

  for (int i = 0; i < MAX_BLIPS; i++) blips[i].active = false;
  for (int i = 0; i < MAX_PINGS; i++) pings[i].active = false;

  splashScreen();
}

void loop() {
  resetNearestPass();
  for (int angle = 15; angle <= 165; angle += 2) {
    updateRadar(angle);
    delay(40);
  }
  resetNearestPass();
  for (int angle = 165; angle >= 15; angle -= 2) {
    updateRadar(angle);
    delay(40);
  }
}

// ---------------------------------------------------------------------
void splashScreen() {
  canvas.fillScreen(COL_BG);
  canvas.drawCircle(centerX, centerY, radius, COL_GRID);
  canvas.setTextColor(COL_SWEEP);
  canvas.setTextSize(1);
  canvas.setCursor(38, 58);
  canvas.print("RADAR ONLINE");
  pushCanvas();
  delay(900);
}

void resetNearestPass() {
  nearestDistancePass = -1;
}

// ---------------------------------------------------------------------
void updateRadar(int angle) {
  radarServo.write(angle);
  long distance = getDistance();

  currentAngle = angle;
  currentDistance = distance;

  if (trailCount < MAX_TRAIL) {
    trailAngles[trailCount++] = angle;
  } else {
    for (int i = 0; i < MAX_TRAIL - 1; i++) trailAngles[i] = trailAngles[i + 1];
    trailAngles[MAX_TRAIL - 1] = angle;
  }

  bool detected = (distance > 0 && distance <= maxDistance);

  if (detected) {
    float rad = radians(angle);
    int objRadius = map(distance, 0, maxDistance, 0, radius);
    int bx = centerX + objRadius * cos(rad);
    int by = centerY - objRadius * sin(rad);
    uint16_t col = distanceColor(distance);
    addBlip(bx, by, col);

    // fire a one-shot ping ring only on the leading edge of a detection
    if (!wasDetectedLastFrame) addPing(bx, by);

    if (nearestDistancePass < 0 || distance < nearestDistancePass) {
      nearestDistancePass = distance;
      nearestX = bx;
      nearestY = by;
    }
  }
  wasDetectedLastFrame = detected;

  updateBuzzer(distance);
  drawFrame();
  ageBlips();
  agePings();
}

// ---------------------------------------------------------------------
void drawFrame() {
  canvas.fillScreen(COL_BG);

  drawGrid();
  drawTrail();
  drawSweepLine();
  drawPings();
  drawBlips();
  drawLockOn();
  drawHUD();

  pushCanvas();
}

void pushCanvas() {
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 160, 128);
}

// ---------------------------------------------------------------------
void drawGrid() {
  canvas.drawCircle(centerX, centerY, radius, COL_GRID_HI);
  canvas.drawCircle(centerX, centerY, radius * 2 / 3, COL_GRID);
  canvas.drawCircle(centerX, centerY, radius / 3, COL_GRID);

  canvas.drawLine(centerX - radius, centerY, centerX + radius, centerY, COL_GRID);
  canvas.drawLine(centerX, centerY, centerX, centerY - radius, COL_GRID);

  for (int a = 0; a <= 180; a += 30) {
    float rad = radians(a);
    int x1 = centerX + (radius - 4) * cos(rad);
    int y1 = centerY - (radius - 4) * sin(rad);
    int x2 = centerX + radius * cos(rad);
    int y2 = centerY - radius * sin(rad);
    canvas.drawLine(x1, y1, x2, y2, COL_GRID_HI);
  }

  canvas.setTextColor(COL_TEXT_DIM);
  canvas.setTextSize(1);
  canvas.setCursor(centerX + 2, centerY - radius / 3 - 6);
  canvas.print(maxDistance / 3);
  canvas.setCursor(centerX + 2, centerY - radius * 2 / 3 - 6);
  canvas.print(maxDistance * 2 / 3);
  canvas.setCursor(centerX + 2, centerY - radius - 6);
  canvas.print(maxDistance);

  canvas.fillCircle(centerX, centerY, 2, COL_SWEEP);
}

void drawTrail() {
  for (int i = 0; i < trailCount - 1; i++) {
    float brightness = (float)(i + 1) / (float)MAX_TRAIL * 0.6;
    float rad = radians(trailAngles[i]);
    int x = centerX + radius * cos(rad);
    int y = centerY - radius * sin(rad);
    canvas.drawLine(centerX, centerY, x, y, fadeColor(COL_SWEEP, brightness));
  }
}

void drawSweepLine() {
  float rad = radians(currentAngle);
  int x = centerX + radius * cos(rad);
  int y = centerY - radius * sin(rad);
  canvas.drawLine(centerX, centerY, x, y, COL_SWEEP);
}

// --- steady blips ---
void addBlip(int x, int y, uint16_t color) {
  for (int i = 0; i < MAX_BLIPS; i++) {
    if (!blips[i].active) {
      blips[i] = { x, y, BLIP_MAX_AGE, color, true };
      return;
    }
  }
  int oldest = 0;
  for (int i = 1; i < MAX_BLIPS; i++) {
    if (blips[i].age < blips[oldest].age) oldest = i;
  }
  blips[oldest] = { x, y, BLIP_MAX_AGE, color, true };
}

void drawBlips() {
  for (int i = 0; i < MAX_BLIPS; i++) {
    if (!blips[i].active) continue;
    float brightness = (float)blips[i].age / (float)BLIP_MAX_AGE;
    int r = blips[i].age > BLIP_MAX_AGE - 4 ? 3 : 2;
    canvas.fillCircle(blips[i].x, blips[i].y, r, fadeColor(blips[i].color, brightness));
  }
}

void ageBlips() {
  for (int i = 0; i < MAX_BLIPS; i++) {
    if (!blips[i].active) continue;
    if (blips[i].age == 0) blips[i].active = false;
    else blips[i].age--;
  }
}

// --- ping rings: one-shot "sonar ping" burst on new detections ---
void addPing(int x, int y) {
  for (int i = 0; i < MAX_PINGS; i++) {
    if (!pings[i].active) {
      pings[i] = { x, y, 0, true };
      return;
    }
  }
}

void drawPings() {
  for (int i = 0; i < MAX_PINGS; i++) {
    if (!pings[i].active) continue;
    float brightness = 1.0 - ((float)pings[i].age / (float)PING_MAX_AGE);
    int r = 2 + pings[i].age * 2;
    canvas.drawCircle(pings[i].x, pings[i].y, r, fadeColor(COL_LOCK, brightness));
  }
}

void agePings() {
  for (int i = 0; i < MAX_PINGS; i++) {
    if (!pings[i].active) continue;
    pings[i].age++;
    if (pings[i].age >= PING_MAX_AGE) pings[i].active = false;
  }
}

// --- lock-on marker for the nearest object in the current pass ---
void drawLockOn() {
  if (nearestDistancePass < 0) return;
  int s = 4;
  canvas.drawRect(nearestX - s, nearestY - s, s * 2, s * 2, COL_LOCK);
  canvas.drawLine(nearestX - s - 3, nearestY, nearestX - s, nearestY, COL_LOCK);
  canvas.drawLine(nearestX + s, nearestY, nearestX + s + 3, nearestY, COL_LOCK);
}

// --- HUD ---
void drawHUD() {
  canvas.setTextSize(1);

  canvas.setTextColor(COL_TEXT);
  canvas.setCursor(4, 4);
  canvas.print("ANG ");
  canvas.print(currentAngle);

  canvas.setCursor(70, 4);
  if (currentDistance > 0 && currentDistance <= maxDistance) {
    canvas.setTextColor(distanceColor(currentDistance));
    canvas.print("DIST ");
    canvas.print(currentDistance);
    canvas.print("cm");
  } else {
    canvas.setTextColor(COL_TEXT_DIM);
    canvas.print("DIST --");
  }

  uint16_t statusColor = (currentDistance > 0 && currentDistance <= maxDistance)
                            ? distanceColor(currentDistance) : COL_FAR;
  canvas.fillCircle(152, 6, 3, statusColor);

  // bottom bar: nearest object this pass
  canvas.drawFastHLine(0, 118, 160, COL_GRID);
  canvas.setCursor(4, 121);
  if (nearestDistancePass >= 0) {
    canvas.setTextColor(distanceColor(nearestDistancePass));
    canvas.print("NEAREST ");
    canvas.print(nearestDistancePass);
    canvas.print("cm");
  } else {
    canvas.setTextColor(COL_TEXT_DIM);
    canvas.print("NEAREST --");
  }
}

// ---------------------------------------------------------------------
// Proximity buzzer: ACTIVE buzzer module (has its own built-in oscillator -
// just needs power to sound, no tone() signal needed). Stays ON
// continuously while an object is within the "near" zone, OFF otherwise.
// ---------------------------------------------------------------------
#define NEAR_THRESHOLD (maxDistance / 3)  // cm - beeps only inside this range

void updateBuzzer(long distance) {
  if (distance > 0 && distance <= NEAR_THRESHOLD) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ---------------------------------------------------------------------
uint16_t distanceColor(long distance) {
  if (distance <= maxDistance / 3) return COL_NEAR;
  if (distance <= (maxDistance * 2) / 3) return COL_MID;
  return COL_FAR;
}

uint16_t fadeColor(uint16_t color, float brightness) {
  if (brightness < 0) brightness = 0;
  if (brightness > 1) brightness = 1;
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (uint8_t)(r * brightness);
  g = (uint8_t)(g * brightness);
  b = (uint8_t)(b * brightness);
  return (r << 11) | (g << 5) | b;
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long distance = duration * 0.034 / 2;

  return distance;
}

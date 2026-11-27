#include <Wire.h>
#include <U8g2lib.h>
#include "MAX30105.h"
#include "heartRate.h"

#define SDA_PIN 5
#define SCL_PIN 6

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
MAX30105 sensor;

// ---- BPM ----
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
int beatAvg = 0;
uint32_t beatFlash = 0;

// ---- Waveform ----
const int W = 72;
const int GRAPH_TOP = 22;
const int GRAPH_BOT = 39;
float wave[W];
float dc = 0, acSmooth = 0;
bool dcInit = false;
uint32_t lastDraw = 0;

void drawHeart(int cx, int cy, bool big) {
  int r = big ? 4 : 3;
  int off = big ? 4 : 3;
  u8g2.drawDisc(cx - off, cy - 1, r);
  u8g2.drawDisc(cx + off, cy - 1, r);
  u8g2.drawTriangle(cx - off - r, cy, cx + off + r, cy, cx, cy + (big ? 9 : 7));
}

void pushWave(long ir) {
  if (!dcInit) { dc = ir; dcInit = true; }
  dc += 0.05f * (ir - dc);               // slow baseline (DC) tracker
  float ac = -(ir - dc);                 // invert so beats point up
  acSmooth += 0.3f * (ac - acSmooth);    // light smoothing
  for (int i = 0; i < W - 1; i++) wave[i] = wave[i + 1];
  wave[W - 1] = acSmooth;
}

void drawWave() {
  float mn = wave[0], mx = wave[0];
  for (int i = 1; i < W; i++) {
    if (wave[i] < mn) mn = wave[i];
    if (wave[i] > mx) mx = wave[i];
  }
  float range = mx - mn;
  if (range < 150) range = 150;          // avoid blowing up noise
  float mid = (mx + mn) / 2;

  int prevY = 0;
  for (int i = 0; i < W; i++) {
    float n = (wave[i] - mid) / range;   // about -0.5 .. 0.5
    int y = (GRAPH_TOP + GRAPH_BOT) / 2 - (int)(n * (GRAPH_BOT - GRAPH_TOP));
    y = constrain(y, GRAPH_TOP, GRAPH_BOT);
    if (i > 0) u8g2.drawLine(i - 1, prevY, i, y);
    prevY = y;
  }
}

void draw(long ir) {
  u8g2.clearBuffer();
  bool finger = ir > 50000;
  bool beatNow = millis() - beatFlash < 180;

  if (!finger) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(4, 14, "Place your");
    u8g2.drawStr(4, 26, "finger...");
    drawHeart(60, 34, false);
  } else {
    drawHeart(9, 7, beatNow);

    u8g2.setFont(u8g2_font_logisoso16_tn);
    char num[5];
    if (beatAvg > 0) snprintf(num, sizeof(num), "%d", beatAvg);
    else snprintf(num, sizeof(num), "--");
    u8g2.drawStr(22, 17, num);

    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(54, 17, "BPM");

    u8g2.drawHLine(0, 20, W);
    drawWave();
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.setPins(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(255);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 14, "Sensor not");
    u8g2.drawStr(0, 26, "found!");
    u8g2.sendBuffer();
    while (1) delay(100);
  }

  sensor.setup();
  sensor.setPulseAmplitudeRed(0x1F);
  sensor.setPulseAmplitudeGreen(0);
  for (int i = 0; i < W; i++) wave[i] = 0;
}

void loop() {
  long ir = sensor.getIR();

  if (checkForBeat(ir)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatFlash = millis();
    float bpm = 60 / (delta / 1000.0);
    if (bpm > 30 && bpm < 220) {
      rates[rateSpot++] = (byte)bpm;
      rateSpot %= RATE_SIZE;
      int sum = 0;
      for (byte i = 0; i < RATE_SIZE; i++) sum += rates[i];
      beatAvg = sum / RATE_SIZE;
    }
  }

  if (ir < 50000) {                      // finger removed: reset
    beatAvg = 0;
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    dcInit = false;
  }

  if (millis() - lastDraw > 40) {        // about 25 fps
    if (ir > 50000) pushWave(ir);
    draw(ir);
    lastDraw = millis();
  }
}

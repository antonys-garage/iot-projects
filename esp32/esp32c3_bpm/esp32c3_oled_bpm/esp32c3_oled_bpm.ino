#include <Wire.h>
#include <U8g2lib.h>
#include "MAX30105.h"
#include "heartRate.h"

#define SDA_PIN 8
#define SCL_PIN 9

// 1.3" OLED: SH1106 128x64. If your screen is SSD1306, use:
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
MAX30105 sensor;

// ---- BPM ----
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
int beatAvg = 0;
uint32_t beatFlash = 0;

// ---- Waveform ----
const int W = 128;
const int GRAPH_TOP = 38;
const int GRAPH_BOT = 63;
float wave[W];
float dc = 0, acSmooth = 0;
bool dcInit = false;
uint32_t lastDraw = 0;

void drawHeart(int cx, int cy, bool big) {
  int r = big ? 7 : 6;
  int off = big ? 7 : 6;
  u8g2.drawDisc(cx - off, cy - 1, r);
  u8g2.drawDisc(cx + off, cy - 1, r);
  u8g2.drawTriangle(cx - off - r, cy, cx + off + r, cy, cx, cy + (big ? 15 : 12));
}

void pushWave(long ir) {
  if (!dcInit) { dc = ir; dcInit = true; }
  dc += 0.05f * (ir - dc);
  float ac = -(ir - dc);                 // inverted so beats point up
  acSmooth += 0.3f * (ac - acSmooth);
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
  if (range < 150) range = 150;
  float mid = (mx + mn) / 2;

  int prevY = 0;
  for (int i = 0; i < W; i++) {
    float n = (wave[i] - mid) / range;
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
    u8g2.setFont(u8g2_font_8x13B_tf);
    u8g2.drawStr(22, 28, "Place finger");
    u8g2.drawStr(22, 46, "on sensor...");
    drawHeart(10, 56, false);
  } else {
    drawHeart(17, 11, beatNow);

    u8g2.setFont(u8g2_font_logisoso24_tn);
    char num[5];
    if (beatAvg > 0) snprintf(num, sizeof(num), "%d", beatAvg);
    else snprintf(num, sizeof(num), "--");
    u8g2.drawStr(46, 30, num);

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(103, 30, "BPM");

    u8g2.drawHLine(0, 34, W);
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
    u8g2.setFont(u8g2_font_8x13B_tf);
    u8g2.drawStr(10, 28, "Sensor not");
    u8g2.drawStr(10, 46, "found!");
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

  if (ir < 50000) {
    beatAvg = 0;
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    dcInit = false;
  }

  if (millis() - lastDraw > 40) {
    if (ir > 50000) pushWave(ir);
    draw(ir);
    lastDraw = millis();
  }
}
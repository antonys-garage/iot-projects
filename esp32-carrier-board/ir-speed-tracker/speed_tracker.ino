#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

/*
================================================================================
📌 SPEED TRACKER SUBSYSTEM WIRING CONNECTIONS
================================================================================
  [Component]         [Component Pin]    [ESP32 Pin]    [Notes]
  ------------------------------------------------------------------------------
  1.3" SH1106 OLED     VCC                3.3V / 5V
                       GND                GND
                       SCL                IO22           Hardware I2C Clock
                       SDA                IO21           Hardware I2C Data
                       
  IR Sensor 1 (Start)  VCC                3.3V / 5V
                       GND                GND
                       OUT                IO32           Hardware Interrupt (Gate 1)
                       
  IR Sensor 2 (Stop)   VCC                3.3V / 5V
                       GND                GND
                       OUT                IO35           Hardware Interrupt (Gate 2)
                       
  3-Pin Buzzer Module  VCC                3.3V / 5V
                       GND                GND
                       I/O (Signal)       IO25           Low-Level Triggered Module
================================================================================
*/

// --- HARDWARE PIN ASSIGNMENTS ---
const int IR_START_PIN = 32; 
const int IR_STOP_PIN  = 35; 
const int BUZZER_PIN   = 25; 

// --- 1.3 INCH OLED INITIALIZATION ---
// Uses default hardware I2C pins: SDA (21), SCL (22)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- CALIBRATION CONFIGURATION ---
const float SENSOR_DISTANCE_CM = 10.0; // Distance between your physical IR gates

// --- VOLATILE VARIABLES FOR HARDWARE INTERRUPTS ---
volatile unsigned long startTime = 0;
volatile unsigned long stopTime  = 0;
volatile bool startTriggered = false;
volatile bool stopTriggered  = false;

// --- STATE MANAGEMENT AND METRICS ---
bool startBeepActive   = false;
unsigned long beepTurnOffMillis = 0;

bool speedCalculated   = false;
float finalSpeedMps    = 0.0;
float finalSpeedKmh    = 0.0;
float finalTimeMs      = 0.0;

// --- INSTANT HARDWARE INTERRUPT SERVICE ROUTINES (ISRs) ---
void IRAM_ATTR onStartSensorTrigger() {
  if (!startTriggered) { 
    startTime = micros(); 
    startTriggered = true;
    
    // Low-Level Active Buzzer Fix: Pull the pin LOW via direct registers instantly 
    // to remove main loop sound latency when the first gate is crossed.
    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << BUZZER_PIN)); 
  }
}

void IRAM_ATTR onStopSensorTrigger() {
  if (startTriggered && !stopTriggered) {
    stopTime = micros(); 
    stopTriggered = true;
  }
}

void showIdleScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 15, "--- RADAR READY ---");
  u8g2.drawStr(10, 35, "Pass object through");
  u8g2.drawStr(10, 50, "gates to measure...");
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  
  // Power-On Beep Fix: Pull HIGH before making it an output to prevent low-level trigger scream
  digitalWrite(BUZZER_PIN, HIGH); 
  pinMode(BUZZER_PIN, OUTPUT);
  
  u8g2.begin();
  showIdleScreen();

  pinMode(IR_START_PIN, INPUT);
  pinMode(IR_STOP_PIN, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(IR_START_PIN), onStartSensorTrigger, FALLING);
  attachInterrupt(digitalPinToInterrupt(IR_STOP_PIN), onStopSensorTrigger, FALLING);
}

void loop() {
  // 1. Non-blocking handler to turn off the instant gate passing beep
  if (startTriggered && !startBeepActive && beepTurnOffMillis == 0) {
    beepTurnOffMillis = millis() + 60; // Sound duration window
    startBeepActive = true;
  }
  
  if (startBeepActive && millis() >= beepTurnOffMillis) {
    digitalWrite(BUZZER_PIN, HIGH); // Pulling HIGH silences low-level module
    startBeepActive = false;
  }

  // 2. Process Gate 2 Exit and Speed Calculation
  if (startTriggered && stopTriggered && !speedCalculated) {
    digitalWrite(BUZZER_PIN, HIGH); // Safety clear line
    
    unsigned long elapsedMicros = stopTime - startTime;
    float elapsedSeconds = elapsedMicros / 1000000.0;
    float distanceMeters = SENSOR_DISTANCE_CM / 100.0;
    
    finalTimeMs = elapsedMicros / 1000.0;
    finalSpeedMps = distanceMeters / elapsedSeconds;
    finalSpeedKmh = finalSpeedMps * 3.6;
    
    speedCalculated = true;

    // Output Gate Success Alert: Clean double beep pattern
    digitalWrite(BUZZER_PIN, LOW);  delay(60);
    digitalWrite(BUZZER_PIN, HIGH); delay(60);
    digitalWrite(BUZZER_PIN, LOW);  delay(60);
    digitalWrite(BUZZER_PIN, HIGH);

    // Display updates
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "--- SPEED DETECTED ---");
    
    u8g2.setCursor(0, 25);
    u8g2.print("Time: "); u8g2.print(finalTimeMs, 1); u8g2.print(" ms");
    
    u8g2.setFont(u8g2_font_logisoso16_tf); 
    u8g2.setCursor(0, 48);
    u8g2.print(finalSpeedKmh, 2);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.print(" km/h");
    
    u8g2.setCursor(0, 62);
    u8g2.print("Speed: "); u8g2.print(finalSpeedMps, 2); u8g2.print(" m/s");
    u8g2.sendBuffer();
    
    delay(2500); 
    
    // System Reset
    startTriggered = false;
    stopTriggered = false;
    speedCalculated = false;
    beepTurnOffMillis = 0;
    
    showIdleScreen();
  }
  
  // 3. Sensor Missing-Gate Timeout Reset (5 Seconds)
  if (startTriggered && !stopTriggered && (micros() - startTime > 5000000)) {
    // Error feedback tone: 1 prolonged warning beep
    digitalWrite(BUZZER_PIN, LOW);  delay(300);
    digitalWrite(BUZZER_PIN, HIGH);
    
    startTriggered = false;
    beepTurnOffMillis = 0;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 35, "[TIMEOUT] Missed Gate 2");
    u8g2.sendBuffer();
    delay(1000);
    showIdleScreen();
  }
}
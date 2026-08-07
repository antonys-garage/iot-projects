#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// PIN CONFIGURATION (ESP32 Carrier Board)
// ==========================================
#define FLAME_PIN       4     // Dedicated Flame Sensor Digital Pin (FLAME_PIN = 4)
#define SPEAKER_PIN     25    // Expansion Breakout Pin connected to Speaker/Audio Amp
#define SDA_PIN         21    // I2C SDA Pin (Expansion Rail)
#define SCL_PIN         22    // I2C SCL Pin (Expansion Rail)

// Initialize the 16x2 I2C LCD (Default address 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);

  // Initialize Pins
  pinMode(FLAME_PIN, INPUT);
  pinMode(SPEAKER_PIN, OUTPUT);

  // Initialize I2C Bus for LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // Initial Welcome Screen
  lcd.setCursor(0, 0);
  lcd.print(" FIRE MONITOR ");
  lcd.setCursor(0, 1);
  lcd.print(" SYSTEM READY ");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Read state from dedicated flame sensor slot (LOW = Flame Detected for most digital modules)
  int flameDetected = digitalRead(FLAME_PIN);

  if (flameDetected == LOW) {
    // --------------------------------------
    // FIRE EMERGENCY DETECTED
    // --------------------------------------
    Serial.println("WARNING: Fire Detected!");

    // Update 16x2 LCD Display
    lcd.setCursor(0, 0);
    lcd.print("** EMERGENCY ** ");
    lcd.setCursor(0, 1);
    lcd.print("FIRE DETECTED! ");

    // Sound High-Decibel Voice/Siren Alarm on Speaker
    playAudioVoiceAlert();

  } else {
    // --------------------------------------
    // SAFE STATE
    // --------------------------------------
    noTone(SPEAKER_PIN); // Mute Speaker
    
    // Update 16x2 LCD Display
    lcd.setCursor(0, 0);
    lcd.print("System Status:  ");
    lcd.setCursor(0, 1);
    lcd.print("SAFE - Scanning ");
    
    delay(200);
  }
}

// Function to synthesize an alternating multi-tone alert announcement on Speaker
void playAudioVoiceAlert() {
  // Sweeping Frequency Voice-Siren Simulation
  for (int hz = 400; hz <= 1200; hz += 40) {
    tone(SPEAKER_PIN, hz);
    delay(5);
  }
  for (int hz = 1200; hz >= 400; hz -= 40) {
    tone(SPEAKER_PIN, hz);
    delay(5);
  }
}
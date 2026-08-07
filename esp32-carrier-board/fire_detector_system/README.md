# Fire Detection System using ESP32, Flame Sensor, 16x2 I2C LCD & PAM8302A

## Overview

This project demonstrates a simple fire detection system using an ESP32 Carrier Board. A digital flame sensor continuously monitors for fire. When a flame is detected, the ESP32:

- Displays an emergency message on a 16×2 I2C LCD.
- Generates a siren alarm through a PAM8302A audio amplifier and speaker.
- Prints warning messages to the Serial Monitor.

---

# Components Required

| Component | Quantity |
|-----------|----------|
| ESP32 Carrier Board | 1 |
| ESP32 Development Board | 1 |
| Flame Sensor Module | 1 |
| 16×2 I2C LCD (I2C) | 1 |
| PAM8302A Audio Amplifier | 1 |
| 8Ω Speaker (1W–3W) | 1 |
| Jumper Wires | As required |
| USB Cable | 1 |

---

# Pin Configuration

| ESP32 GPIO | Device |
|------------|--------|
| GPIO4 | Flame Sensor DO |
| GPIO21 | LCD SDA |
| GPIO22 | LCD SCL |
| GPIO25 | PAM8302A Audio Input (A+) |

---

# Wiring Connections

## Flame Sensor

| Flame Sensor | ESP32 |
|--------------|-------|
| VCC | 3.3V |
| GND | GND |
| DO | GPIO4 |

> **Note:** This project uses the Digital Output (DO) of the flame sensor.

---

## 16×2 I2C LCD

| LCD Pin | ESP32 |
|----------|-------|
| VCC | 5V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Default I2C Address:

- `0x27`
- Some modules use `0x3F`

---

## PAM8302A Audio Amplifier

| PAM8302A Pin | ESP32 Connection |
|---------------|-----------------|
| VIN | 5V |
| GND | GND |
| A+ | GPIO25 |
| A− | GND |
| SHDN | 3.3V (Always Enabled) |

---

## Speaker Connection

| Speaker | PAM8302A |
|----------|----------|
| Positive (+) | SPK+ |
| Negative (-) | SPK− |

---

# Complete Wiring Diagram

```text
                     ESP32 Carrier Board
                  +-----------------------+
                  |                       |
 GPIO4   ---------+ Flame Sensor DO       |
 GPIO21  ---------+ LCD SDA               |
 GPIO22  ---------+ LCD SCL               |
 GPIO25  ---------+ PAM8302A A+           |
 GND     ---------+ PAM8302A GND/A-       |
 GND     ---------+ LCD GND               |
 GND     ---------+ Flame Sensor GND      |
 5V      ---------+ LCD VCC               |
 5V      ---------+ PAM8302A VIN          |
 3.3V    ---------+ Flame Sensor VCC      |
                  +-----------------------+

                 PAM8302A Amplifier
              +------------------------+
              | VIN   -> 5V            |
              | GND   -> GND           |
              | A+    -> GPIO25        |
              | A-    -> GND           |
              | SHDN  -> 3.3V          |
              |                        |
              | SPK+  -> Speaker +     |
              | SPK-  -> Speaker -     |
              +------------------------+
```

---

# Working Principle

1. The ESP32 continuously monitors the flame sensor.
2. When a flame is detected, the sensor outputs **LOW**.
3. The LCD displays **FIRE DETECTED**.
4. The ESP32 generates a siren using `tone()`.
5. The PAM8302A amplifies the audio signal and drives the speaker.
6. When no flame is detected, the LCD shows **SAFE - Scanning** and the alarm stops.

---

# LCD Output

## Normal Operation

```
System Status:
SAFE - Scanning
```

## Fire Detected

```
** EMERGENCY **
FIRE DETECTED!
```

---

# Serial Monitor Output

## Safe

```
System Running...
```

## Fire Detected

```
WARNING: Fire Detected!
```

---

# Libraries Required

Install the following libraries from the Arduino Library Manager:

- Wire
- LiquidCrystal_I2C

---

# Arduino IDE Settings

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| Flash Frequency | 80 MHz |
| Partition Scheme | Default |
| Port | Your ESP32 COM Port |

---

# Features

- Digital flame detection
- 16×2 I2C LCD status display
- Loud siren using PAM8302A amplifier
- Serial Monitor logging
- Simple and beginner-friendly wiring
- Expandable for IoT applications

---

# Future Improvements

- Wi-Fi notifications
- Telegram alerts
- Blynk IoT integration
- GSM SMS alerts
- Relay-controlled water pump
- RGB status LEDs
- Smoke sensor (MQ-2)
- Temperature sensor
- SD card logging
- Cloud dashboard

---

# License

This project is open-source and may be used for educational and personal projects.

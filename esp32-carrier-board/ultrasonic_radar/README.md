# ESP32 TFT Radar Display

A feature-rich ultrasonic radar system built on the ESP32 platform, featuring a servo-driven sweep, real-time object detection with visual effects, and a flicker-free ST7735 TFT display.

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Pin Connections](#pin-connections)
4. [Libraries Required](#libraries-required)
5. [Software Architecture](#software-architecture)
6. [Visual Effects System](#visual-effects-system)
7. [Configuration](#configuration)
8. [Code Reference](#code-reference)
9. [Troubleshooting](#troubleshooting)

---

## Overview

This project transforms an ESP32 and a handful of common components into a cinematic radar display. An HC-SR04 ultrasonic sensor mounted on a servo sweeps 15°–165°, measuring distances and rendering them onto a 160×128 pixel ST7735 TFT screen with:

- **Flicker-free rendering** via an off-screen framebuffer (`GFXcanvas16`)
- **Sweep trail** showing recent scan history
- **Persistent blips** that fade gradually after detection
- **Sonar ping rings** that burst outward on new detections
- **Lock-on brackets** tracking the nearest object per sweep pass
- **Proximity buzzer** for audible near-range alerts

---

## Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|-------|
| Microcontroller | ESP32 DevKit | Any ESP32 variant with SPI & PWM |
| Display | ST7735 1.8" TFT | 160×128 resolution, SPI interface |
| Ultrasonic Sensor | HC-SR04 | 2–400 cm range |
| Servo Motor | SG90 or compatible | 5V tolerant, 50 Hz PWM |
| Buzzer | Active buzzer module | Built-in oscillator (not a passive piezo) |
| Power | 3.3V / 5V | ESP32 logic at 3.3V; servo may need separate 5V supply |

---

## Pin Connections

### TFT Display → ESP32

| TFT Pin | ESP32 Pin | Function |
|---------|-----------|----------|
| VCC | 3.3V | Main power supply |
| GND | GND | Ground |
| CS | IO22 | Chip Select (SPI) |
| RESET | IO21 | Screen Reset |
| A0 (DC / RS) | IO17 | Data / Command selection |
| SDA (MOSI) | IO23 | SPI Master Out Slave In |
| SCK (CLK) | IO18 | SPI Clock |
| LED (BL) | IO19 | Backlight control (HIGH = On) |

### Sensor & Actuators → ESP32

| Component | ESP32 Pin | Function |
|-----------|-----------|----------|
| HC-SR04 Trig | IO15 | Ultrasonic trigger |
| HC-SR04 Echo | IO13 | Ultrasonic echo |
| Servo Signal | IO5 | PWM servo control |
| Buzzer (+) | IO4 | Active buzzer drive (optional) |

> **Note:** The buzzer pin can be left unwired if the audible alert is not needed.

---

## Libraries Required

Install the following via the Arduino Library Manager:

1. **Adafruit GFX Library** — Core graphics primitives
2. **Adafruit ST7735 and ST7789 Library** — ST7735 driver
3. **ESP32Servo** — ESP32-compatible servo control with PWM timer allocation

All three are available in the Arduino IDE under **Sketch → Include Library → Manage Libraries**.

---

## Software Architecture

### Framebuffer Strategy

The display is never drawn to directly. Instead, every frame is composed in RAM on a `GFXcanvas16` (160×128, 16-bit color) and then blasted to the TFT in a single SPI burst via `drawRGBBitmap()`. This eliminates flicker and allows layering of transparency-style effects without tearing.

### Main Loop Flow

```
Loop (bidirectional sweep)
├── Forward pass:  15° → 165° (step 2°)
├── Reverse pass: 165° →  15° (step 2°)
│
└── Per-angle updateRadar(angle)
    ├── Move servo to angle
    ├── Read ultrasonic distance
    ├── Update sweep trail buffer
    ├── If object detected:
    │   ├── Compute screen coordinates
    │   ├── Add blip (persistent dot)
    │   ├── Add ping ring (if new detection edge)
    │   └── Track nearest object
    ├── Update buzzer state
    ├── Draw complete frame
    └── Age blips & pings
```

---

## Visual Effects System

### 1. Sweep Trail
A circular buffer stores the last 6 sweep angles. Older positions are drawn with reduced brightness, creating a fading "comet tail" behind the active sweep line.

### 2. Blips (Persistent Detections)
When an object is detected, a blip is placed at its polar coordinates. Blips persist for `BLIP_MAX_AGE` frames (default 18), gradually dimming until they expire. Up to 10 blips are maintained; the oldest is overwritten when the pool is full.

### 3. Ping Rings (Sonar Burst)
A one-shot expanding ring fires only on the **leading edge** of a new detection — i.e., when the previous frame had no object but the current frame does. The ring expands for `PING_MAX_AGE` frames (default 10) while fading out.

### 4. Lock-On Brackets
During each half-sweep pass, the nearest detected object is marked with a white square bracket. The bracket resets at the start of each pass.

### 5. Color-Coded Distance
| Zone | Distance | Color | Hex |
|------|----------|-------|-----|
| Near | 0–13 cm | Red | `0xF800` |
| Mid  | 14–26 cm | Amber | `0xFDA0` |
| Far  | 27–40 cm | Green | `0x07E0` |

All HUD elements (angle, distance, status LED, nearest readout) reflect these colors in real time.

---

## Configuration

### Tunable Constants

| Constant | Default | Description |
|----------|---------|-------------|
| `maxDistance` | 40 cm | Maximum detection range displayed |
| `MAX_TRAIL` | 6 | Length of sweep trail |
| `MAX_BLIPS` | 10 | Concurrent persistent blips |
| `BLIP_MAX_AGE` | 18 | Frames a blip remains visible |
| `MAX_PINGS` | 4 | Concurrent ping rings |
| `PING_MAX_AGE` | 10 | Frames a ping ring expands |
| `NEAR_THRESHOLD` | `maxDistance / 3` | Buzzer activation distance |

### Servo Calibration

The servo is attached with a 50 Hz period and pulse range 500–2400 µs. If your servo exhibits jitter or limited travel, adjust:

```cpp
radarServo.attach(SERVO_PIN, 500, 2400);
```

### Display Orientation

```cpp
tft.setRotation(3);
```

Change the rotation value (0–3) to match your physical mounting.

---

## Code Reference

### Core Functions

| Function | Purpose |
|----------|---------|
| `setup()` | Initializes SPI, PWM timers, display, servo, and sensors |
| `loop()` | Drives the bidirectional servo sweep |
| `updateRadar(angle)` | Single-angle update: servo, sensor, effects, render |
| `drawFrame()` | Composes the full framebuffer and pushes to TFT |
| `getDistance()` | HC-SR04 trigger/echo pulse with 30 ms timeout |
| `addBlip(x, y, color)` | Registers a new persistent detection dot |
| `addPing(x, y)` | Registers a new sonar burst ring |
| `updateBuzzer(distance)` | Activates buzzer inside near threshold |
| `distanceColor(distance)` | Maps distance to near/mid/far color |
| `fadeColor(color, brightness)` | Scales RGB565 color by a float 0.0–1.0 |

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Blank screen | Backlight off / wrong pins | Verify `TFT_BL` wired to IO19 and `digitalWrite(TFT_BL, HIGH)` in setup |
| Garbled display | Wrong initialization tab | Ensure `tft.initR(INITR_BLACKTAB)` matches your ST7735 variant |
| No servo movement | PWM timer conflict | Confirm `ESP32PWM::allocateTimer(0..3)` calls precede servo attach |
| Erratic distances | Echo pin noise / low voltage | Add a 100 µF cap near HC-SR04 VCC/GND; ensure 5V supply |
| Buzzer always on | Passive buzzer used | Replace with **active** buzzer, or swap to `tone()` logic |
| Slow frame rate | `delay(40)` per angle | Reduce to `delay(20)` for faster sweep (may increase servo jitter) |

---

## License & Credits

This project is provided as open-source reference material. It builds upon:

- **Adafruit Industries** — GFX & ST7735 libraries
- **ESP32Servo** — ESP32 PWM servo implementation

---

*Document generated for the ESP32 TFT Radar project.*

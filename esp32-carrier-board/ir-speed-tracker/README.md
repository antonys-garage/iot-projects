# ESP32 Speed Tracker

A high-precision, interrupt-driven speed measurement system built on the ESP32 platform. It uses two infrared (IR) gate sensors to calculate the velocity of a passing object and displays the results on a 1.3" SH1106 OLED display.

---

## Features

- **Microsecond-level precision** using hardware interrupts and `micros()` timestamps
- **Non-blocking buzzer feedback** with distinct audio patterns for gate pass, success, and timeout events
- **Instant start beep** via direct GPIO register manipulation to eliminate loop latency
- **Timeout protection** — automatically resets if the second gate is not triggered within 5 seconds
- **Dual-unit display** — shows speed in both km/h and m/s, plus elapsed time in milliseconds
- **Active-low buzzer compatibility** with power-on squeal prevention

---

## Hardware Requirements

| Component | Specification | Quantity |
|-----------|---------------|----------|
| Microcontroller | ESP32 DevKit | 1 |
| OLED Display | 1.3" SH1106, I2C | 1 |
| IR Sensor Module | TCRT5000 or similar (3-pin) | 2 |
| Buzzer Module | 3-pin, Active Low | 1 |
| Jumper Wires | Dupont M-F / M-M | As needed |
| Breadboard | — | Optional |

---

## Wiring Diagram

### 1.3" SH1106 OLED (I2C)

| OLED Pin | ESP32 Pin | Notes |
|----------|-----------|-------|
| VCC | 3.3V / 5V | Power supply |
| GND | GND | Common ground |
| SCL | GPIO 22 | Hardware I2C Clock |
| SDA | GPIO 21 | Hardware I2C Data |

### IR Sensor 1 — Start Gate

| Sensor Pin | ESP32 Pin | Notes |
|------------|-----------|-------|
| VCC | 3.3V / 5V | Power supply |
| GND | GND | Common ground |
| OUT | GPIO 32 | Hardware Interrupt (Gate 1) |

### IR Sensor 2 — Stop Gate

| Sensor Pin | ESP32 Pin | Notes |
|------------|-----------|-------|
| VCC | 3.3V / 5V | Power supply |
| GND | GND | Common ground |
| OUT | GPIO 35 | Hardware Interrupt (Gate 2) |

### 3-Pin Buzzer Module

| Buzzer Pin | ESP32 Pin | Notes |
|------------|-----------|-------|
| VCC | 3.3V / 5V | Power supply |
| GND | GND | Common ground |
| I/O (Signal) | GPIO 25 | Low-Level Triggered |

> **Note:** The buzzer module used in this project is **active-low**, meaning pulling the signal pin `LOW` activates the sound and `HIGH` silences it.

---

## Pinout Summary

| Function | GPIO | Direction |
|----------|------|-----------|
| I2C SDA | 21 | Output |
| I2C SCL | 22 | Output |
| IR Start Sensor | 32 | Input (Interrupt) |
| IR Stop Sensor | 35 | Input (Interrupt) |
| Buzzer Signal | 25 | Output |

---

## How It Works

1. **Idle State** — The system waits on the idle screen (`--- RADAR READY ---`).
2. **Gate 1 Trigger** — When an object breaks the first IR beam, a hardware interrupt fires:
   - `startTime` is captured via `micros()`.
   - The buzzer is activated instantly via direct register write (`GPIO_OUT_W1TC_REG`).
3. **Gate 2 Trigger** — When the object breaks the second IR beam:
   - `stopTime` is captured.
   - Elapsed time is computed: `elapsed = stopTime - startTime`.
   - Speed is calculated as `distance / time` and converted to **m/s** and **km/h**.
4. **Display & Feedback** — Results are shown on the OLED, and a double-beep success tone plays.
5. **Auto-Reset** — After 2.5 seconds, the system returns to idle.
6. **Timeout** — If Gate 2 is not triggered within 5 seconds, a timeout warning beeps and the system resets.

---

## Calibration

The only physical parameter you need to configure is the distance between the two IR gates.

```cpp
const float SENSOR_DISTANCE_CM = 10.0;  // Distance between IR gates in centimeters
```

Measure the exact center-to-center (or beam-to-beam) distance between your two sensors and update this value before uploading.

---

## Software Dependencies

This project requires the following Arduino libraries:

| Library | Purpose | Install via |
|---------|---------|-------------|
| [U8g2](https://github.com/olikraus/u8g2) | OLED display driver | Arduino Library Manager (`U8g2` by olikraus) |

### Installation Steps

1. Open **Arduino IDE**.
2. Go to **Sketch → Include Library → Manage Libraries...**
3. Search for `U8g2` by **olikraus**.
4. Click **Install**.
5. Select your **ESP32 Dev Module** board from **Tools → Board**.
6. Upload the sketch.

---

## Usage

1. **Power on** the ESP32. The OLED should show the idle screen.
2. **Pass an object** through Gate 1, then Gate 2.
3. **Listen** for the audio cues:
   - Short beep → Object passed Gate 1.
   - Double beep → Speed measured successfully.
   - Long beep → Timeout (missed Gate 2).
4. **Read** the speed and time on the OLED display.

---

## Code Architecture

### Interrupt Service Routines (ISRs)

Both sensor pins are configured with `FALLING` edge interrupts. The ISRs are marked with `IRAM_ATTR` to ensure they execute from ESP32 internal RAM, avoiding flash cache misses.

```cpp
void IRAM_ATTR onStartSensorTrigger() { ... }
void IRAM_ATTR onStopSensorTrigger() { ... }
```

### Volatile Variables

All variables shared between ISRs and the main loop are declared `volatile` to prevent compiler optimization issues:

```cpp
volatile unsigned long startTime = 0;
volatile unsigned long stopTime  = 0;
volatile bool startTriggered = false;
volatile bool stopTriggered  = false;
```

### Non-Blocking Buzzer Control

The start beep uses a timestamp-based (`millis()`) off-switch rather than `delay()`, ensuring the main loop remains responsive:

```cpp
if (startTriggered && !startBeepActive && beepTurnOffMillis == 0) {
    beepTurnOffMillis = millis() + 60;
    startBeepActive = true;
}
```

### Register-Level GPIO (Start Beep)

To eliminate the latency between ISR entry and `digitalWrite()`, the code uses ESP32's `GPIO_OUT_W1TC_REG` (Write 1 To Clear) register to pull GPIO 25 low instantly:

```cpp
REG_WRITE(GPIO_OUT_W1TC_REG, (1 << BUZZER_PIN));
```

---

## Troubleshooting

| Issue | Possible Cause | Solution |
|-------|--------------|----------|
| OLED stays blank | Wrong I2C address or wiring | Verify SDA → 21, SCL → 22. Try scanning I2C addresses. |
| No response from sensors | Incorrect pin or power issue | Check VCC/GND. Verify OUT pins are 32 and 35. |
| Buzzer screams on boot | Pin initialized as OUTPUT while LOW | The code handles this by writing `HIGH` before `pinMode()`. Ensure your wiring matches. |
| Inconsistent speed readings | Object not moving straight or gate distance inaccurate | Ensure sensors are aligned. Calibrate `SENSOR_DISTANCE_CM` precisely. |
| False triggers | Ambient IR light or vibration | Add small debounce capacitors or increase physical shielding around sensors. |

---

## Safety Notes

- The IR sensors operate at 3.3V–5V logic levels. The ESP32 GPIOs are **3.3V tolerant only** — do not exceed this on input pins.
- Ensure the active-low buzzer module includes a built-in transistor/driver. Do not connect a raw piezo buzzer directly to GPIO 25 without a current-limiting resistor or driver circuit.

---

## License

This project is open-source. Modify and distribute freely for personal or educational use.

---

## Author

Built for the ESP32 platform using the Arduino framework.

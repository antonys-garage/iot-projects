# ESP32-C3 SuperMini Heart Rate Monitor (1.3" OLED + MAX30102)

A compact heart rate monitor built on the ESP32-C3 SuperMini. A MAX30102 sensor
reads your pulse, and a 1.3" 128x64 OLED shows a big BPM number, a heart that pulses
on every beat, and a live scrolling pulse graph.

> **Hardware used:** ESP32-C3 SuperMini, **MAX30102** sensor breakout, and an
> **external 1.3" I2C OLED (128x64, SH1106)**.

## Features

- Live BPM (averaged over the last 4 beats)
- Large heart icon that grows on every detected beat
- Full-width scrolling pulse waveform
- "Place finger" prompt when no finger is detected
- Serial output of raw IR values for debugging

## Hardware

| Part | Notes |
|---|---|
| ESP32-C3 SuperMini | USB-C, no onboard display |
| MAX30102 breakout | I2C address `0x57` |
| 1.3" I2C OLED, 128x64 | Usually **SH1106**, address `0x3C` (some are SSD1306) |
| Jumper wires / breadboard | |

### Wiring

Both devices share the same two I2C pins.

| OLED | SuperMini |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

| MAX30102 | SuperMini |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |
| INT | not connected |

> Power everything from **3.3V**, not 5V.
>
> GPIO8 is also the onboard LED and GPIO9 is the BOOT button. If you see flaky
> behaviour, move both devices to **GPIO4 (SDA)** and **GPIO5 (SCL)** and change
> `SDA_PIN` / `SCL_PIN` in the sketch.

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and the **ESP32** board package.
2. Install these libraries from **Library Manager**:
   - **SparkFun MAX3010x Pulse and Proximity Sensor Library**
   - **U8g2** (olikraus)
3. Select the board **ESP32C3 Dev Module**.
4. Set **USB CDC On Boot: Enabled** (otherwise the Serial Monitor stays blank).
5. Open `esp32c3_supermini_max30102_oled13.ino` and upload.

Serial Monitor baud rate: `115200`.

### Choosing the right OLED driver

Most 1.3" modules use the **SH1106** controller. The sketch uses:

```cpp
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
```

If your display is **SSD1306** (the image looks shifted by 2 pixels or has noise at the
edge on SH1106), use this instead:

```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
```

## Usage

1. Power the board. The screen shows **Place finger on sensor...**
2. Rest a fingertip **lightly** on the sensor window. Do not press hard.
3. Hold still. The BPM settles after about 10 to 20 seconds.

## Configuration

| What | Where | Default |
|---|---|---|
| I2C pins | `SDA_PIN`, `SCL_PIN` | `8`, `9` |
| OLED driver | `u8g2` constructor | SH1106 |
| Finger detection threshold | `ir > 50000` | `50000` |
| Red LED brightness | `setPulseAmplitudeRed()` | `0x1F` |
| Graph sensitivity | `range < 150` in `drawWave()` | `150` |
| Graph speed | draw interval `40` ms in `loop()` | `40` |
| Waveform upside down | sign in `pushWave()`: `-(ir - dc)` | inverted |

## Troubleshooting

**Blank screen**
- Run an I2C scanner with `Wire.begin(8, 9)`. The OLED should appear at `0x3C`
  (a few modules use `0x3D`).
- Check VCC on 3V3 and that SDA/SCL aren't swapped.

**Garbled or shifted image**
- The display is probably SSD1306, not SH1106. Switch the constructor as shown above.

**"Sensor not found"**
- Scan the bus. You should see `0x3C` and `0x57`.
- Reflow the header pins on the sensor. Loose joints are common.
- Some green MAX30100-style boards have pull-ups tied to a 1.8V rail. Remove the two
  `472` (4.7k) resistors on the module.
- Confirm the chip: read register `0xFF` at address `0x57`. `0x15` means MAX30102.
  This project uses the **SparkFun MAX3010x** library, which is for the **MAX30102**.

**Random I2C addresses in the scan**
- SDA or SCL is floating. Recheck the wires and add 4.7k pull-ups to 3V3 if the
  OLED module has none.

**IR value stays near 1000 with a finger on**
- The finger isn't covering the sensor window. Raise LED current with
  `setPulseAmplitudeIR(0x3F)` and `setPulseAmplitudeRed(0x3F)` if the signal is weak.

**BPM jumps around**
- Hold still, use light pressure, and shield the sensor from bright light.

## Notes

- This is a hobby project and **not a medical device**. Do not use it for diagnosis.
- SpO2 is not shown. It needs the library's `spo2_algorithm.h` and is far less stable
  than BPM on these sensors.

## Credits

Built by **Antony's Garage** (@code_antony).
Libraries: SparkFun MAX3010x, U8g2.

# ESP32 Color Sorting Machine

A ball sorting machine built on the **NodeMCU ESP32 Carrier Board V1**. A TCS34725 sensor reads the colour of a ball, a BO motor turns a four-chamber tray to the right position, a servo opens a lid, and the ball drops into its chamber. If the colour isn't recognised, a buzzer sounds and the lid stays closed.

Built by **Antony's Garage** (@code_antony).

---

## Features

- Sorts **red, green, yellow and blue** balls into four chambers
- Uses the carrier board's built-in motor driver and sensor header, so wiring is just jumper wires
- Fixed tray positions measured from a **home** position, and the tray returns home after every ball
- **Error alarm:** if the colour isn't recognised, the buzzer sounds, the lid stays closed and the tray stays put
- Serial **calibration** for your own balls, saved to flash
- Everything tunable from `#define` lines at the top of the sketch

---

## How it works

```
Ball dropped into basket
        |
TCS34725 detects the ball (clear-channel level rises)
        |
Reads the colour (up to 3 readings)
        |
   +----+----------------------+
   |                           |
Recognised                 Not recognised
   |                           |
Tray turns to the chamber   Buzzer beeps, lid stays closed
Lid opens, ball drops       Remove the ball to continue
Lid closes
Tray returns home
```

### Tray positions

The tray starts with the **red chamber under the chute (home)**. Each colour moves the tray a fixed number of quarter-turns from home:

| Colour | Move from home | After the drop |
|---|---|---|
| Red | none | none |
| Blue | 1 step forward | 1 step back |
| Yellow | 2 steps forward | 2 steps back |
| Green | 1 step reverse | 1 step forward |

Change the layout with this line in the sketch (order: red, green, yellow, blue):

```cpp
const int MOVE_STEPS[4] = {0, -1, 2, 1};
```

---

## Hardware

| Part | Notes |
|---|---|
| NodeMCU ESP32 Carrier Board V1 | with an ESP32 DevKit V1 (ESP-WROOM-32) plugged in |
| TCS34725 RGB colour sensor | I2C |
| Servo (SG90 type) | opens and closes the lid |
| BO motor with wheel | turns the tray by friction against its rim |
| Buzzer module | low-level trigger type (MH-FMD) |
| Coloured balls | red, green, yellow, blue |
| Cardboard, tape, glue | post, basket, chute, and a 4-chamber tray |
| Power | battery or DC supply into the carrier board (USB alone will not power the servo or motor) |

### Mechanical build

1. A **post** of cardboard on a base box.
2. A small **square basket** at the top, open at the top, with a **lid at the bottom** driven by the servo. The colour sensor is mounted on the **inside wall** of the basket.
3. A **chute** below the lid that guides the ball into the tray.
4. A **circular tray** with four partitions, one per colour, turning on a central pivot. The BO motor's wheel presses against the tray's rim.

---

## Wiring (jumper wires)

| Part | Connect to | Pin |
|---|---|---|
| Servo | `SERVO_M` connector | IO5 |
| BO motor | `MOTOR-1` screw terminal | IN1 = 27, IN2 = 26, ENA = 14 |
| TCS34725 | TCS34725 header: 5V/3V3, GND, SCL, SDA | SDA = 21, SCL = 22 |
| Buzzer module | VCC to the **3V** pin, GND to GND, I/O to **IO16** (ESP32 header) | IO16 |

**DIP switches:** turn `SERVO_SW` (3) and `TCS_SW` (5) ON.

**Buzzer note:** the module is a low-level-trigger type. Power it from **3.3V** so the ESP32's HIGH level switches it fully off. The sketch holds IO16 HIGH when idle and pulls it LOW to beep.

---

## Software setup

1. Install the **Arduino IDE** and the **ESP32 board package** (Espressif, 3.x).
2. Install these libraries from Library Manager:
   - `Adafruit TCS34725` (and `Adafruit BusIO` if it asks)
   - `ESP32Servo`
3. Select the board **ESP32 Dev Module** and the right COM port.
4. Open `color_sorter_v6.ino` and upload.
5. Open the Serial Monitor at **115200 baud**.

---

## First run

1. **Home position:** turn the tray by hand so the **red chamber is under the chute** before powering on.
2. **Empty basket:** power on with the basket empty and the lid closed. The sketch measures the empty-basket light level at start-up.
3. **Calibrate the colours:** put one ball in front of the sensor and send its letter in the Serial Monitor:

   | Send | Ball |
   |---|---|
   | `r` | red |
   | `g` | green |
   | `y` | yellow |
   | `b` | blue |

   The values are saved to flash and stay after a restart. Send `p` to print them.
4. **Tune the tray step:** send `f` to move one step forward and `v` for one step back. The tray should move exactly one chamber. Adjust `STEP_FWD_MS` and `STEP_REV_MS` until it does.

### Serial commands

| Command | What it does |
|---|---|
| `r` `g` `y` `b` | calibrate that colour with the ball in front of the sensor |
| `s` | print a live sensor reading |
| `p` | print the saved calibration |
| `f` | move the tray one step forward |
| `v` | move the tray one step in reverse |

---

## Settings

All near the top of the sketch.

| Setting | Default | What it does |
|---|---|---|
| `MOTOR_PWM` | 170 | tray motor power (0 to 255) |
| `KICK_MS` | 40 | full-power kick at the start of each move |
| `STEP_FWD_MS` / `STEP_REV_MS` | 700 | time for one quarter-turn forward and in reverse |
| `RETURN_HOME` | 1 | 1 = tray returns home after each ball |
| `MOVE_STEPS` | `{0, -1, 2, 1}` | tray moves from home for red, green, yellow, blue |
| `GATE_CLOSED` / `GATE_OPEN` | 100 / 10 | servo angles; swap them if your lid moves the wrong way |
| `GATE_OPEN_MS` | 800 | how long the lid stays open |
| `SETTLE_MS` | 600 | wait for the ball to stop before reading |
| `DETECT_FACTOR` / `DETECT_OFFSET` | 1.25 / 20 | ball is detected above `baseline x factor + offset` |
| `MAX_MATCH_DIST` | 0.12 | larger colour distance = unrecognised |
| `RECHECK_TRIES` | 2 | extra readings before the buzzer sounds |
| `BUZZER_PIN` | 16 | buzzer module I/O pin |
| `BUZZER_ENABLED` | 1 | 0 turns the buzzer off completely |
| `BENCH_TEST` | 0 | 1 disables the motor for sensor and servo testing |

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Servo doesn't move | Connect the battery or DC supply (USB won't power it), switch the board on, and turn `SERVO_SW` ON |
| Motor doesn't turn | Same power checks. Raise `MOTOR_PWM` or `KICK_MS`, and make sure `BENCH_TEST` is 0 |
| Sensor not found at start-up | Check SDA/SCL wiring and turn `TCS_SW` ON |
| Every ball gives the buzzer | Calibrate the colours (`r` `g` `y` `b`) and check `MAX_MATCH_DIST` is `0.12f` |
| Ball isn't detected | Bring the ball within about 5 to 10 mm of the sensor, and check that the clear reading rises above the threshold in the Serial Monitor |
| Wrong chamber | Tune `STEP_FWD_MS` / `STEP_REV_MS` with `f` and `v`, and check the red chamber is under the chute at power-on |
| Buzzer beeps all the time | Use the 3V pin for VCC (not 5V) and check the sketch's `BUZZER_PIN` matches the pin the I/O wire is on |
| Lid moves the wrong way | Swap the values of `GATE_CLOSED` and `GATE_OPEN` |

---

## Known limits

- The tray moves on **timed steps**, not measured positions, so wheel slip or a low battery can make it drift over many balls. Check that the red chamber is still under the chute before a long run.
- Red and yellow are the colours most likely to be confused, so calibrate with your actual balls under steady light.

---

## Project files

```
color_sorter_v6/
  color_sorter_v6.ino   main sketch
README.md
```

---

## Author

**Antony's Garage** (@code_antony). Maker and electronics projects, tutorials and reels.

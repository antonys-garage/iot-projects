# ESP32 4WD Wi-Fi Web Remote Car

A standalone, browser-controlled 4WD robot car powered by an **ESP32 Dev Module** and an **L298N Dual H-Bridge Motor Driver**. The ESP32 hosts its own local Wi-Fi Access Point (SoftAP) and serves an interactive web dashboard with **PWM variable speed control** and **touch-responsive steering** directly to any web browser—**no mobile apps, internet connection, or external routers required**.

---

## Features

- **Self-Hosted Local Web Server:** Broadcasts its own private Wi-Fi network (`ESP32-Car`) and serves an embedded HTML5 touch controller.
- **Variable PWM Speed Control:** Configurable 8-bit hardware PWM via modern `ledcAttach` API, allowing real-time speed adjustments (duty cycle 80–255) via an interactive slider.
- **Skid-Steer / Differential Drive:** Left and right motor pairs are driven independently for full forward, backward, spot-left, and spot-right rotation.
- **Zero-Lag Touch Controls:** Uses multi-touch JavaScript listeners (`touchstart` / `touchend` with active interval pinging) to provide continuous motion while holding a button, with automatic stop upon release.
- **Failsafe Watchdog:** Built-in auto-stop cutoff brakes the motors if no valid command packet is received within 500 ms (protects against Wi-Fi dropouts).
- **Decoupled Power Architecture:** Completely isolates the microcontroller logic supply from inductive motor draw to prevent brownouts and reboot loops.

---

## Bill of Materials (BOM)

| Component | Quantity | Description / Role |
| :--- | :---: | :--- |
| **ESP32 Development Board** | 1 | Microcontroller running SoftAP & Web Server |
| **L298N Motor Driver Module** | 1 | Dual H-Bridge motor driver (PWM capable) |
| **Double-Layer 4WD Acrylic Chassis** | 1 | Dual-deck chassis, mounting standoffs, and hardware |
| **Yellow BO Motors (3–6V DC) & Wheels** | 4 | Skid-steer drivetrain (wired in parallel pairs) |
| **2S Lithium Battery Pack (7.4V – 8.4V)** | 1 | Dedicated high-current supply for the motors |
| **5V USB Power Bank** | 1 | Clean, isolated logic supply for the ESP32 |
| **1000 µF / 25V Electrolytic Capacitor** | 1 | Filters motor inrush noise across the battery rail |
| **Jumper Wires** | — | Logic and ground interconnects |

---

## Hardware Pinout & Wiring

### 1. Motor Wiring (Skid-Steer Parallel Pairs)
- **Left Motors (Front + Rear):** Wired in parallel to L298N **OUT1** and **OUT2**.
- **Right Motors (Front + Rear):** Wired in parallel to L298N **OUT3** and **OUT4**.

### 2. Logic & PWM Control Pins
> ⚠️ **Important:** Remove both black jumper caps from the **ENA** and **ENB** header pins on the L298N board so the ESP32 can feed hardware PWM speed signals directly.

| L298N Driver Pin | ESP32 GPIO | Function |
| :--- | :--- | :--- |
| **ENA** | `GPIO 25` | PWM Speed Control (Left Motors) |
| **IN1** | `GPIO 26` | Left Motors Direction 1 |
| **IN2** | `GPIO 27` | Left Motors Direction 2 |
| **IN3** | `GPIO 32` | Right Motors Direction 1 |
| **IN4** | `GPIO 33` | Right Motors Direction 2 |
| **ENB** | `GPIO 14` | PWM Speed Control (Right Motors) |
| **GND** | `GND` | **Common Ground Reference (Mandatory)** |

### 3. Power Architecture & Brownout Protection
> ⚡ **Workbench Note:** Do not power the ESP32 from the L298N's built-in 5V pin. When all 4 BO motors start under load, inductive inrush current pulls down the battery rail, crashing the driver's internal linear regulator and triggering the ESP32 hardware **Brownout Detector (BOD)**.

- **Motor Supply:** Connect the 2S Lithium battery pack positive (+) and negative (-) leads directly to the L298N **12V** and **GND** screw terminals.
- **Capacitor Buffer:** Install a **1000 µF 25V electrolytic capacitor** directly across the L298N **12V (+)** and **GND (-)** terminals (observe polarity) to absorb back-EMF voltage transients.
- **ESP32 Supply:** Power the ESP32 via its onboard USB port from an isolated 5V USB power bank.
- **Common Ground:** Connect a ground wire from the L298N **GND** terminal to an ESP32 **GND** pin.

---

## Arduino Source Code (`esp32_4wd_car.ino`)

```cpp
// ==========================================
// Pin Connections:
// L298N        ESP32 GPIO
// ENA          25 (PWM speed, left side)
// IN1          26
// IN2          27
// IN3          32
// IN4          33
// ENB          14 (PWM speed, right side)
// GND          GND (Common Ground is mandatory)
// ==========================================

#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-Car";
const char* pass = "12345678";

#define ENA 25
#define IN1 26
#define IN2 27
#define IN3 32
#define IN4 33
#define ENB 14

WebServer server(80);
int speedVal = 200;  // 0 - 255
unsigned long lastCmd = 0;

void setMotors(int l, int r) {  // Range: -255 to 255
  digitalWrite(IN1, l > 0 ? HIGH : LOW);
  digitalWrite(IN2, l < 0 ? HIGH : LOW);
  digitalWrite(IN3, r > 0 ? HIGH : LOW);
  digitalWrite(IN4, r < 0 ? HIGH : LOW);

  // Write PWM duty cycle directly to pins
  ledcWrite(ENA, abs(l));
  ledcWrite(ENB, abs(r));
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>ESP32 Web Remote</title>
<style>
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    text-align: center;
    background: #111;
    color: #eee;
    user-select: none;
    -webkit-user-select: none;
    touch-action: none;
    margin: 0;
    padding-top: 20px;
  }
  h2 { margin-bottom: 15px; }
  .grid {
    display: grid;
    grid-template-columns: repeat(3, 90px);
    gap: 12px;
    justify-content: center;
    margin: 20px auto;
  }
  button {
    height: 90px;
    font-size: 28px;
    border: 0;
    border-radius: 14px;
    background: #2a6;
    color: #fff;
    touch-action: manipulation;
  }
  button:active { background: #e83; }
  .stop { background: #c33; }
  .stop:active { background: #911; }
  .slider-box { margin-top: 15px; }
  input[type=range] { width: 75%; height: 10px; margin-top: 10px; }
</style>
</head>
<body>
  <h2>ESP32 Car</h2>
  <div class="grid">
    <span></span><button data-c="F">&#9650;</button><span></span>
    <button data-c="L">&#9664;</button><button class="stop" data-c="S">&#9632;</button><button data-c="R">&#9654;</button>
    <span></span><button data-c="B">&#9660;</button><span></span>
  </div>

  <div class="slider-box">
    Speed: <span id="v">200</span><br>
    <input type="range" min="80" max="255" value="200" id="sp">
  </div>

<script>
let t = null;
function send(c) { fetch('/move?c=' + c).catch(()=>{}); }

document.querySelectorAll('button').forEach(b => {
  const c = b.dataset.c;
  const start = e => {
    e.preventDefault();
    send(c);
    if (c !== 'S') {
      clearInterval(t);
      t = setInterval(() => send(c), 150);
    }
  };
  const end = e => {
    e.preventDefault();
    clearInterval(t);
    send('S');
  };
  b.addEventListener('touchstart', start, {passive: false});
  b.addEventListener('touchend', end, {passive: false});
  b.addEventListener('mousedown', start);
  b.addEventListener('mouseup', end);
});

const sp = document.getElementById('sp');
const v = document.getElementById('v');
sp.oninput = () => {
  v.textContent = sp.value;
  fetch('/speed?v=' + sp.value).catch(()=>{});
};
</script>
</body>
</html>
)rawliteral";

void handleMove() {
  String c = server.arg("c");
  int s = speedVal;
  lastCmd = millis();

  if (c == "F")      setMotors(s, s);
  else if (c == "B") setMotors(-s, -s);
  else if (c == "L") setMotors(-s, s);
  else if (c == "R") setMotors(s, -s);
  else               setMotors(0, 0);

  server.send(200, "text/plain", "ok");
}

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Configure 1 kHz PWM with 8-bit resolution (0-255)
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);
  setMotors(0, 0);

  WiFi.softAP(ssid, pass);

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/move", handleMove);
  server.on("/speed", []() {
    speedVal = constrain(server.arg("v").toInt(), 0, 255);
    server.send(200, "text/plain", "ok");
  });
  server.begin();
}

void loop() {
  server.handleClient();

  // Failsafe: stop motors if no command received for 500 ms
  if (millis() - lastCmd > 500) {
    setMotors(0, 0);
  }
}
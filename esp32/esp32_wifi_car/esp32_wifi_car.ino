// L298N	ESP32 GPIO
// ENA	  25 (PWM speed, left side)
// IN1	  26
// IN2	  27
// IN3	  32
// IN4	  33
// ENB	  14 (PWM speed, right side)
// GND	  GND (common ground is a must)

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
int speedVal = 200;  // 0-255
unsigned long lastCmd = 0;

void setMotors(int l, int r) {  // -255..255
  digitalWrite(IN1, l > 0); digitalWrite(IN2, l < 0);
  digitalWrite(IN3, r > 0); digitalWrite(IN4, r < 0);
  ledcWrite(ENA, abs(l));
  ledcWrite(ENB, abs(r));
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
body{font-family:sans-serif;text-align:center;background:#111;color:#eee;user-select:none;touch-action:none}
.grid{display:grid;grid-template-columns:repeat(3,90px);gap:10px;justify-content:center;margin:20px auto}
button{height:90px;font-size:28px;border:0;border-radius:14px;background:#2a6;color:#fff}
button:active{background:#e83}
.stop{background:#c33}
input{width:80%}
</style></head><body>
<h2>ESP32 Car</h2>
<div class="grid">
<span></span><button data-c="F">▲</button><span></span>
<button data-c="L">◀</button><button class="stop" data-c="S">■</button><button data-c="R">▶</button>
<span></span><button data-c="B">▼</button><span></span>
</div>
Speed: <span id="v">200</span><br>
<input type="range" min="80" max="255" value="200" id="sp">
<script>
let t=null;
function send(c){fetch('/move?c='+c).catch(()=>{});}
document.querySelectorAll('button').forEach(b=>{
  const c=b.dataset.c;
  const start=e=>{e.preventDefault();send(c);if(c!='S'){clearInterval(t);t=setInterval(()=>send(c),150);}};
  const end=e=>{e.preventDefault();clearInterval(t);send('S');};
  b.addEventListener('touchstart',start);b.addEventListener('touchend',end);
  b.addEventListener('mousedown',start);b.addEventListener('mouseup',end);
});
sp.oninput=()=>{v.textContent=sp.value;fetch('/speed?v='+sp.value);};
</script></body></html>
)rawliteral";

void handleMove() {
  String c = server.arg("c");
  int s = speedVal;
  lastCmd = millis();
  if (c == "F") setMotors(s, s);
  else if (c == "B") setMotors(-s, -s);
  else if (c == "L") setMotors(-s, s);
  else if (c == "R") setMotors(s, -s);
  else setMotors(0, 0);
  server.send(200, "text/plain", "ok");
}

void setup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
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
  // failsafe: stop if no command for 500 ms
  if (millis() - lastCmd > 500) setMotors(0, 0);
}
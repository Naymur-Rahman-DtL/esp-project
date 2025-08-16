// Full ESP8266 Radar Project with:
// - OLED display
// - Ultrasonic sensor & Servo motor
// - Buzzer for alarm
// - Web interface
// - Wi-Fi Mode toggle (AP <-> STA)

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SERVO_PIN D5
#define TRIG_PIN D6
#define ECHO_PIN D7
#define BUZZER_PIN D8
#define LASER_PIN D4

Servo myServo;
ESP8266WebServer server(80);

const char* apSSID = "Radar-AP";
const char* apPASS = "00000000";
char staSSID[32] = "";
char staPASS[32] = "";

int angle = 0;
int step = 2;
long distanceCM = 0;
bool isAPMode = true;

void saveWiFiCredentials(const char* ssid, const char* pass) {
  EEPROM.begin(512);
  for (int i = 0; i < 32; ++i) EEPROM.write(i, ssid[i]);
  for (int i = 0; i < 32; ++i) EEPROM.write(32 + i, pass[i]);
  EEPROM.commit();
  EEPROM.end();
}

void loadWiFiCredentials() {
  EEPROM.begin(512);
  for (int i = 0; i < 32; ++i) staSSID[i] = EEPROM.read(i);
  for (int i = 0; i < 32; ++i) staPASS[i] = EEPROM.read(32 + i);
  EEPROM.end();
}

void clearWiFiCredentials() {
  EEPROM.begin(512);
  for (int i = 0; i < 64; ++i) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();
}

void setupWiFi() {
  loadWiFiCredentials();
  if (strlen(staSSID) > 0) {
    WiFi.begin(staSSID, staPASS);
    Serial.println("Connecting to saved Wi-Fi...");
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 10) {
      delay(1000);
      Serial.print(".");
      tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      isAPMode = false;
      Serial.println("\nConnected to Wi-Fi: " + WiFi.localIP().toString());
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(apSSID, apPASS);
    Serial.println("\nStarted AP mode: 192.168.4.1");
    isAPMode = true;
  }
}

void setup() {
  Serial.begin(115200);
  myServo.attach(SERVO_PIN);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LASER_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (true);
  }

  setupWiFi();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setwifi", handleSetWiFi);
  server.on("/resetwifi", handleResetWiFi);
  server.onNotFound([]() {
    server.send(200, "text/html", "<meta http-equiv='refresh' content='0; url=/' />");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  myServo.write(angle);
  delay(25);
  distanceCM = readDistance();
  if (distanceCM < 20) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
  }

  if (distanceCM < 30) {
    digitalWrite(LASER_PIN, HIGH);
    delay(50);
    digitalWrite(LASER_PIN, LOW);
  }

  drawRadarDisplay(angle, distanceCM);
  angle += step;
  if (angle >= 180 || angle <= 0) step = -step;
}

long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return constrain(duration * 0.034 / 2, 0, 400);
}

void drawRadarDisplay(int angle, int distance) {
  display.clearDisplay();
  int cx = 64, cy = 63, r = 60;
  for (int i = 10; i <= r; i += 10) display.drawCircle(cx, cy, i, SSD1306_WHITE);
  for (int a = 0; a <= 180; a += 30) {
    float rad = radians(a);
    int x = cx + cos(rad) * r;
    int y = cy - sin(rad) * r;
    display.drawLine(cx, cy, x, y, SSD1306_WHITE);
  }
  float rad = radians(angle);
  int sx = cx + cos(rad) * r;
  int sy = cy - sin(rad) * r;
  display.drawLine(cx, cy, sx, sy, SSD1306_WHITE);
  int d = constrain(distance, 0, r);
  int bx = cx + cos(rad) * d;
  int by = cy - sin(rad) * d;
  display.fillCircle(bx, by, 2, SSD1306_WHITE);

  // Show distance text in cm on OLED
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Dist: ");
  display.print(distance);
  display.print(" cm");

  display.display();
}

void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html><html><head><title>ESP Radar</title></head>
    <body style='background:black; color:lime; text-align:center;'>
    <h2>Radar Interface</h2>
    <canvas id="radar" width="300" height="300"></canvas>
    <div id='stats' style='margin:10px auto; font-family:Arial; color:#0f0; font-size:18px;'>
      Angle: <span id='ang'>0</span>° &nbsp; | &nbsp; Distance: <span id='dist'>0</span> cm
    </div>
    <br>
    <form method='POST' action='/setwifi'>
      <input name='ssid' placeholder='WiFi SSID'><br>
      <input name='pass' type='password' placeholder='Password'><br>
      <button type='submit'>Switch to Wi-Fi Mode</button>
    </form>
    <form method='POST' action='/resetwifi'><button>Reset Wi-Fi</button></form>
    <script>
    const ctx = radar.getContext('2d');
    const cx = 150, cy = 300, r = 140;
    function draw(angle, distance) {
      ctx.clearRect(0, 0, 300, 300);
      ctx.strokeStyle = 'lime';
      for (let i = 50; i <= r; i += 30) {
        ctx.beginPath(); ctx.arc(cx, cy, i, Math.PI, 2*Math.PI); ctx.stroke();
      }
      for (let a = 0; a <= 180; a += 30) {
        const rad = a * Math.PI / 180;
        ctx.beginPath(); ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(rad)*r, cy - Math.sin(rad)*r); ctx.stroke();
      }
      const rad = angle * Math.PI / 180;
      const bx = cx + Math.cos(rad) * distance * 4;
      const by = cy - Math.sin(rad) * distance * 4;
      ctx.beginPath(); ctx.moveTo(cx, cy);
      ctx.lineTo(bx, by); ctx.strokeStyle='red'; ctx.stroke();
      ctx.beginPath(); ctx.arc(bx, by, 4, 0, 2 * Math.PI);
      ctx.fillStyle = 'lime'; ctx.fill();
    }
    setInterval(() => {
      fetch('/data').then(r => r.json()).then(d => {
        draw(d.angle, d.distance);
        document.getElementById('ang').textContent = d.angle;
        document.getElementById('dist').textContent = d.distance;
      });
    }, 300);
    </script></body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"angle\":" + String(angle) + ",";
  json += "\"distance\":" + String(distanceCM);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetWiFi() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String s = server.arg("ssid");
    String p = server.arg("pass");
    saveWiFiCredentials(s.c_str(), p.c_str());
    server.send(200, "text/html", "Saved. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing SSID or PASS");
  }
}

void handleResetWiFi() {
  clearWiFiCredentials();
  server.send(200, "text/html", "Wi-Fi credentials cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

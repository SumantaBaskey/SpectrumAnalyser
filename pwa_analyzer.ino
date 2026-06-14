/*
 * ESP32 Dumb LCD Display for audioMotion PWA Analyzer
 *
 * Connects to phone hotspot → HTTP server on port 80
 * Endpoint: GET /s?d=0,1,2,...,15  → parse 16 values (0-16) → update 16x2 LCD
 *
 * Dependencies: LiquidCrystal_I2C library (Frank de Brabander)
 * Wiring: SDA=GPIO21, SCL=GPIO22, VCC=5V, GND=GND, I2C addr=0x27
 *
 * Configure SSID/PASSWORD below for your phone's hotspot.
 * If STA fails, falls back to AP mode "AudioMotion-LCD" at 192.168.4.1
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── WiFi Configuration ─────────────────────────────────────────────────────
// Set to your phone hotspot credentials
const char* WIFI_SSID     = "AudioMotion";
const char* WIFI_PASSWORD = "spectrum16";

const int   WIFI_TIMEOUT  = 15000; // ms before falling back to AP mode

// ── LCD ─────────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Custom characters (CGRAM slots 1-7): N rows filled from bottom (height N/8)
static const byte barChars[7][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F },
  { 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F },
  { 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
  { 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
  { 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F }
};

// Double-buffer: only write changed cells (reduces I2C traffic)
char curTop[16];
char curBot[16];

// ── HTTP Server ────────────────────────────────────────────────────────────
WebServer server(80);

// ── Helpers ────────────────────────────────────────────────────────────────

// Map height 0-16 to LCD character:
//   0 → space, 1-7 → custom char, 8 → solid block (0xFF)
static char heightToChar(int h) {
  if (h <= 0) return ' ';
  if (h >= 8) return (char)0xFF;
  return (char)h;
}

// Write a single cell only if changed
static void writeCell(int col, int row, char ch, char* buf) {
  if (buf[col] != ch) {
    lcd.setCursor(col, row);
    lcd.write((uint8_t)ch);
    buf[col] = ch;
  }
}

// Update LCD from 16 bar heights (0-16)
static void updateLCD(const byte* bars) {
  for (int c = 0; c < 16; c++) {
    byte v = bars[c];
    char t = ' ', b = ' ';
    if (v >= 9) { b = (char)0xFF; t = heightToChar(v - 8); }
    else        { b = heightToChar(v); }
    writeCell(c, 0, t, curTop);
    writeCell(c, 1, b, curBot);
  }
}

// ── HTTP handlers ──────────────────────────────────────────────────────────

void handleRoot() {
  String html = "<html><body style='background:#111;color:#0f0;font-family:monospace;padding:20px;'>";
  html += "<h2>audioMotion LCD</h2>";
  html += "<p>ESP32 IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p>Send bars: <code>GET /s?d=0,1,2,...,15</code></p>";
  html += "<p>Values: 0-16 per bar</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSend() {
  if (!server.hasArg("d")) {
    server.send(400, "text/plain", "Missing d parameter. Use /s?d=0,1,2,...,15");
    return;
  }

  String data = server.arg("d");
  byte bars[16];
  int idx = 0;
  int start = 0;

  for (int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data.charAt(i) == ',') {
      if (idx < 16) {
        String part = data.substring(start, i);
        part.trim();
        int val = part.toInt();
        if (val < 0) val = 0;
        if (val > 16) val = 16;
        bars[idx++] = (byte)val;
      }
      start = i + 1;
    }
  }

  // Pad remaining bars with 0 if fewer than 16 values received
  while (idx < 16) bars[idx++] = 0;

  updateLCD(bars);
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found. Use / or /s");
}

// ── Setup ──────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);

  // ── LCD init ──
  Wire.begin(21, 22);
  Wire.setClock(400000);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  for (int i = 0; i < 7; i++)
    lcd.createChar(i + 1, (byte*)barChars[i]);

  for (int c = 0; c < 16; c++) { curTop[c] = ' '; curBot[c] = ' '; }

  lcd.setCursor(0, 0); lcd.print("Connecting WiFi ");
  lcd.setCursor(0, 1); lcd.print(WIFI_SSID);

  // ── WiFi STA mode (connect to phone hotspot) ──
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  bool connected = false;

  while (millis() - startTime < WIFI_TIMEOUT) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(500);
  }

  if (connected) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("IP:");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
    Serial.print("Connected to "); Serial.println(WIFI_SSID);
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
  } else {
    // Fallback to AP mode
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("AP mode");
    lcd.setCursor(0, 1); lcd.print("AudioMotion-LCD");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AudioMotion-LCD");
    Serial.println("STA failed. AP mode: AudioMotion-LCD");
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("AP: AudioMotion");
    lcd.setCursor(0, 1); lcd.print("IP: ");
    lcd.print(WiFi.softAPIP().toString());
  }

  // ── HTTP routes ──
  server.on("/", handleRoot);
  server.on("/s", handleSend);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP server started on port 80");
  delay(2000);
}

// ── Loop ───────────────────────────────────────────────────────────────────

void loop() {
  server.handleClient();
}

# audioMotion PWA + ESP32 LCD Spectrum Analyzer

All FFT processing runs in the browser (phone/laptop). ESP32 is a dumb display — receives 16 bar heights via HTTP and updates a 16×2 LCD.

## How it works

```
GitHub Pages (CDN) ← phone loads page via mobile data
         ↕
┌──────────────────┐
│  Phone Browser   │ ← opens GitHub URL
│  audioMotion.js  │ ← FFT, computes 16 bars
│  GET /s?d=...    │ ← HTTP to ESP32
└────────┬─────────┘
         │ same WiFi (phone's hotspot)
    ┌────┴────┐
    │  ESP32  │ ← connects to hotspot
    │  port   │ ← displays IP on LCD
    │  parse  │ ← /s?d=0,1,...15 → update LCD
    │  no FFT │ ← dumb receiver
    └─────────┘
```

## Files

| File | Purpose |
|------|---------|
| `pwa_analyzer.ino` | ESP32 firmware — WiFi + HTTP + LCD |
| `index.html` | Web UI — full audioMotion + ESP32 streaming |
| `manifest.json` | PWA manifest for "Add to Home Screen" |

## Setup

### 1. GitHub Pages (host the web UI)

1. Push this folder to a GitHub repo
2. Go to repo **Settings → Pages** → select branch as source
3. Your page will be at `https://<user>.github.io/<repo>/pwa_analyzer/`
4. Open that URL on your phone

### 2. ESP32 firmware

1. Open `pwa_analyzer.ino` in Arduino IDE
2. Install library: **LiquidCrystal_I2C** (Frank de Brabander)
3. Set your phone's hotspot SSID/password at the top:
   ```cpp
   const char* WIFI_SSID     = "YourPhoneHotspot";
   const char* WIFI_PASSWORD = "yourpassword";
   ```
4. Select board: **ESP32 Dev Module** (or your specific board)
5. Upload to ESP32
6. Open Serial Monitor (115200 baud) to see connection status

### 3. Wiring

| ESP32 | LCD (I2C) |
|-------|-----------|
| GPIO21 | SDA |
| GPIO22 | SCL |
| 5V | VCC |
| GND | GND |

Default I2C address: `0x27` (change in code to `0x3F` if needed)

### 4. Usage

1. Power the ESP32 — it connects to your phone's hotspot and displays its IP on the LCD
2. Open the GitHub Pages URL on your phone
3. Enter the ESP32 IP shown on the LCD into the "ESP32 IP" field
4. Tap **Connect** — status turns green
5. Play audio (upload a file, enable mic, or capture tab audio)
6. The spectrum displays on both the phone canvas and the physical LCD

### Offline mode (AP mode)

If the ESP32 can't connect to your hotspot (e.g., no saved credentials), it falls back to AP mode:
- SSID: `AudioMotion-LCD`
- IP: `192.168.4.1`
- Connect your phone to this WiFi, then open the IP in a browser

**Note:** In AP mode the phone has no internet, so the page must be cached (PWA) or loaded beforehand.

## ⚠️ Mixed Content (HTTPS → HTTP)

The web UI uses `Image()` requests to send data from a GitHub Pages (HTTPS) page to the ESP32 (HTTP). This is "passive mixed content" — most browsers allow it by default.

- **Chrome**: Shows a ⚠️ shield in the address bar. If bars aren't updating, tap the shield and select "Load unsafe scripts" (or "Allow mixed content" via the site info icon → Site settings → Insecure content → Allow).
- **Safari**: Usually loads without issue.
- **Firefox**: Usually loads without issue.

## Settings

All settings from audioMotion-analyzer are available in the collapsible panel:

- **Mode**: bar resolution (1/24 octave to full octave)
- **FFT**: fftSize, smoothing
- **Frequency**: min/max frequency, scale type (log/linear/bark/mel)
- **Sensitivity**: weighting filter, min/max dB
- **Linear amplitude**: toggle + boost
- **Display**: bar spacing, channel layout, color mode, gradient, mirror, reflex
- **Toggle buttons**: ledBars, lumiBars, showPeaks, scale, radial, FPS, etc.

## ESP32 LCD protocol

ESP32 exposes a single HTTP endpoint:

```
GET /s?d=val0,val1,val2,...,val15
```

Each `val` is an integer 0–16:
- 0 = empty
- 1–7 = bottom row partial fill
- 8 = bottom row full
- 9–15 = bottom full + top row partial
- 16 = both rows full

The browser calls this endpoint at the configured FPS rate (default 30).

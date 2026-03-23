# ScreenMetrics

An ESP8266 sketch that displays metrics on an OLED screen. Metrics are persisted to EEPROM and survive resets.

## Hardware

- **Board**: ESP8266 (NodeMCU, Wemos D1 Mini, or similar)
- **Display**: SSD1306 OLED (0.96" - 128x64 pixels, I2C address 0x3C)
- **Connections**:
  - SDA -> D1 (GPIO5)
  - SCL -> D2 (GPIO4)
  - VCC -> 3.3V
  - GND -> GND

## Dependencies

Install these libraries via Arduino Library Manager:

- Adafruit SSD1306
- Adafruit GFX
- WiFiManager by Tzapu

## Features

- **EEPROM Persistence**: Metrics are saved to flash and survive reboots/resets
- **WiFi Optional**: Works without WiFi; when available, HTTP API is enabled
- **AP Mode**: When no saved WiFi is found, creates AP "ScreenMetrics"
- **mDNS**: Access via `http://screenmetrics.local` when on WiFi
- **HTTP API**: Control metrics via HTTP requests
- **OLED Display**: Shows one metric at a time, auto-scrolls every 5 seconds
- **Network Display**: Shows IP address when connected to WiFi, "ScreenMetrics" when in AP mode

## HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/set?key=NAME&value=VALUE` | GET | Set or update a metric |
| `/list` | GET | List all metrics |
| `/clear` | GET | Remove all metrics |
| `/status` | GET | Show device status |

### Examples

```bash
curl "http://screenmetrics.local/set?key=BUILD&value=PASSING"
curl "http://192.168.1.100/set?key=SEV2s&value=3"
curl "http://screenmetrics.local/list"
curl "http://screenmetrics.local/clear"
```

## First Time Setup

The ESP8266 will create an AP named "ScreenMetrics" on first boot if no WiFi is configured.

1. Connect to WiFi network "ScreenMetrics" (password: `12345678`)
2. Open browser to `http://192.168.4.1`
3. Select your WiFi network and enter password
4. Device will reboot and connect to your WiFi

Or use the Python setup command:

```bash
python screenmetrics.py --setup
```

## Display Behavior

- Shows one metric at a time
- Auto-scrolls every 5 seconds
- Displays metric name (centered, top) and value (large, below separator)
- Page indicator at bottom left (e.g., P1/2)
- Bottom right shows:
  - **IP address** (e.g., `192.168.1.100`) when connected to WiFi
  - **"ScreenMetrics"** when in AP mode (no saved WiFi)
- Maximum 20 metrics supported
- Metric name truncated to 10 characters, value to 10 characters

## Serial Commands

Serial commands are available at 115200 baud for debugging:

| Command | Description |
|---------|-------------|
| `SET\|KEY\|VALUE` | Set or update a metric |
| `DEL\|KEY` | Delete a metric |
| `LIST` | List all metrics |
| `CLEAR` | Remove all metrics |

**Note:** Metrics are persisted to EEPROM immediately on every change, so serial resets don't affect them.

## Reset WiFi Settings

If you need to reconnect to a different WiFi network, add this in `setup()` before `wifiManager.autoConnect()`:

```cpp
wifiManager.resetSettings();
```

Then upload the sketch again - it will create the AP for reconfiguration.

## Limitations

### Serial Port Reset Issue

The ESP8266 resets when the serial port is opened due to DTR (Data Terminal Ready) signal behavior. When any serial terminal connects (Arduino IDE Serial Monitor, Python script, etc.), DTR goes HIGH and the ESP8266 bootloader interprets this as a reset trigger.

**Symptoms:**
- Opening the serial port causes the ESP8266 to reboot
- Each command sent may cause unexpected resets
- Fast consecutive commands may not complete before reset

**This does NOT affect metrics** - they are persisted to EEPROM immediately on every change and survive the reset.

**Serial commands are for debugging only.** Use the HTTP API for reliable metric updates.

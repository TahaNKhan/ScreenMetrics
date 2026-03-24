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

## Architecture

The codebase follows clean code principles with single-responsibility classes:

| File | Responsibility |
|------|---------------|
| `ScreenMetrics.ino` | Entry point, wires components together |
| `MetricsStore.h/.cpp` | Metric CRUD + EEPROM persistence |
| `DisplayManager.h/.cpp` | OLED rendering + auto-scroll |
| `ApiServer.h/.cpp` | HTTP REST API |
| `SerialCLI.h/.cpp` | Serial command processing |

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
- **Network Display**: Shows IP address when connected to WiFi

## HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | API documentation (HTML) |
| `/set?key=NAME&value=VALUE` | GET | Set or update a metric |
| `/delete?key=NAME` | GET | Remove a metric by key |
| `/list` | GET | List all metrics |
| `/clear` | GET | Remove all metrics |
| `/status` | GET | Show device status |

### Examples

```bash
# Set a metric
curl "http://192.168.1.100/set?key=BUILD&value=PASSING"

# Delete a metric
curl "http://192.168.1.100/delete?key=BUILD"

# List all metrics
curl "http://192.168.1.100/list"

# Clear all metrics
curl "http://192.168.1.100/clear"

# Check status
curl "http://192.168.1.100/status"
```

## First Time Setup

The ESP8266 will create an AP named "ScreenMetrics" on first boot if no WiFi is configured.

1. Connect to WiFi network "ScreenMetrics" (password: `12345678`)
2. Open browser to `http://192.168.4.1`
3. Select your WiFi network and enter password
4. Device will reboot and connect to your WiFi

## Display Behavior

- Shows one metric at a time
- Auto-scrolls every 5 seconds
- Displays metric name (centered, top) and value (large, below separator)
- Page indicator at bottom left (e.g., P1/2)
- Bottom right shows the device IP address
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

## Reset WiFi Settings

If you need to reconnect to a different WiFi network, add this before `autoConnect()`:

```cpp
WiFiManager wm;
wm.resetSettings();
```

Then upload the sketch again — it will create the AP for reconfiguration.

## Limitations

### Serial Port Reset Issue

The ESP8266 resets when the serial port is opened due to DTR signal behavior. When any serial terminal connects, DTR goes HIGH and the ESP8266 bootloader interprets this as a reset trigger.

**This does NOT affect metrics** — they are persisted to EEPROM immediately on every change and survive the reset.

**Serial commands are for debugging only.** Use the HTTP API for reliable metric updates.

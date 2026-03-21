# ScreenMetrics

An Arduino sketch that displays metrics on an OLED screen with serial command control.

## Hardware

- **Board**: Arduino (Uno, Nano, ESP32, ESP8266, or similar)
- **Display**: SSD1306 OLED (0.96" - 128x64 pixels, I2C address 0x3C)
- **Connections**:
  - SDA -> A4 (or SDA pin on your board)
  - SCL -> A5 (or SCL pin on your board)
  - VCC -> 5V or 3.3V
  - GND -> GND

## Dependencies

Install these libraries via Arduino Library Manager:
- Adafruit SSD1306
- Adafruit GFX

## Commands

Send commands via Serial (9600 baud):

| Command | Description |
|---------|-------------|
| `SET\|KEY\|VALUE` | Set or update a metric |
| `DEL\|KEY` | Delete a metric |
| `LIST` | List all metrics via serial |
| `CLEAR` | Remove all metrics |

### Examples

```
SET|BUILD|PASSING
SET|SEV2 TICKET COUNT|3
DEL|BUILD
LIST
CLEAR
```

## Python Client

Install dependencies:
```bash
pip install -r requirements.txt
```

### As a Library

```python
from screenmetrics import ScreenMetrics, find_port, list_available_ports

# Auto-detect port (default 9600 baud for Arduino)
port = find_port()
if port:
    with ScreenMetrics(port) as sm:
        sm.set("BUILD", "PASSING")

# For ESP32/ESP8266 use 115200 baud
with ScreenMetrics("COM3", baudrate=115200) as sm:
    sm.set("BUILD", "PASSING")

# Or use with explicit port
with ScreenMetrics("COM3") as sm:
    sm.set("BUILD", "PASSING")
    sm.set("CPU", "45%")
    print(sm.list())
```

### CLI Usage

```bash
# Auto-detect port (recommended for Arduino - 9600 baud)
python screenmetrics.py -s BUILD PASSING
python screenmetrics.py -l

# For ESP32/ESP8266 (115200 baud)
python screenmetrics.py -b 115200 -s BUILD PASSING

# Specify port manually
python screenmetrics.py -p COM3 -s BUILD PASSING
python screenmetrics.py -p /dev/ttyUSB0 -b 115200 -s CPU 45%

# List available ports
python screenmetrics.py --list-ports
```

## Display Behavior

- Shows one metric at a time
- Auto-scrolls every 5 seconds
- Displays metric name (centered, top) and value (large, below separator)
- Page indicator at bottom left (e.g., P1/2)
- Maximum 20 metrics supported
- Metric name truncated to 10 characters, value to 10 characters

## Default Metrics

On startup, two metrics are pre-populated:
- BUILD: PENDING
- SEV2s: 0

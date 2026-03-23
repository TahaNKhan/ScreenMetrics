#!/usr/bin/env python3
"""Python client for ScreenMetrics Arduino OLED display."""

import serial
import time
from serial.tools import list_ports


def find_port() -> str | None:
    """Auto-detect ScreenMetrics board port.

    Looks for:
    1. Known USB-to-Serial VID/PIDs (Arduino, ESP32, ESP8266)
    2. Port descriptions containing known board names
    3. Fallback: first available serial port

    Returns:
        Port name if found, None otherwise
    """
    ports = list_ports.comports()

    # Known USB-to-Serial VID/PIDs
    known_vids = {
        "2341",  # Arduino
        "0403",  # FTDI / FT232
        "10C4",  # Silicon Labs CP210x (ESP32, Arduino)
        "1A86",  # CH340 (ESP8266, Arduino clones)
        "067B",  # PL2303
        "FTDI",  # Some FTDI devices
    }

    # Known board keywords in description
    board_keywords = [
        "arduino", "esp32", "esp8266", "ch340", "ch341",
        "cp210", "ft232", "ftdi", "pl2303", "uno", "nano",
        "mega", "nano", "promicro", "nodeMCU", "wemos"
    ]

    for port in ports:
        # Check VID
        if port.vid:
            vid = format(port.vid, '04X')
            if vid in known_vids:
                return port.device

        # Check description
        desc = (port.description or "").lower()
        if any(keyword in desc for keyword in board_keywords):
            return port.device

    # Fallback: return first serial port
    if ports:
        return ports[0].device

    return None


def list_available_ports() -> list[dict]:
    """List available serial ports with info.

    Returns:
        List of dicts with port, description, hwid
    """
    ports = list_ports.comports()
    return [
        {"port": p.device, "description": p.description, "hwid": p.hwid}
        for p in ports
    ]


class ScreenMetrics:
    """Send metrics to ScreenMetrics device (Arduino, ESP32, ESP8266)."""

    def __init__(self, port: str, baudrate: int = 9600, timeout: float = 1.0):
        """Initialize serial connection.

        Args:
            port: Serial port (e.g., 'COM3' on Windows, '/dev/ttyUSB0' on Linux)
            baudrate: Communication speed (9600 for Arduino, 115200 for ESP32/ESP8266)
            timeout: Read timeout in seconds
        """
        self.serial = serial.Serial(port, baudrate, timeout=timeout)
        # Disable DTR/RTS to prevent ESP8266 reset on open
        self.serial.dtr = False
        self.serial.rts = False
        # Wait longer for ESP32/ESP8266 which boot slower
        wait_time = 2 if baudrate <= 9600 else 3
        time.sleep(wait_time)

    def set(self, key: str, value: str) -> str:
        """Set or update a metric.

        Args:
            key: Metric name
            value: Metric value

        Returns:
            Response from device
        """
        return self._send(f"SET|{key}|{value}")

    def delete(self, key: str) -> str:
        """Delete a metric.

        Args:
            key: Metric name to delete

        Returns:
            Response from device
        """
        return self._send(f"DEL|{key}")

    def list(self) -> str:
        """List all metrics.

        Returns:
            Response from device with all metrics
        """
        return self._send("LIST")

    def clear(self) -> str:
        """Clear all metrics.

        Returns:
            Response from device
        """
        return self._send("CLEAR")

    def _send(self, command: str) -> str:
        """Send command and read response."""
        c = f"{command}\r\n"
        self.serial.write(c.encode('ascii'))
        time.sleep(1)

        lines = []
        while self.serial.in_waiting:
            line = self.serial.readline().decode('utf-8', errors='replace').strip()
            if line:
                lines.append(line)
        print('\n'.join(lines))
        return "\n".join(lines)

    def close(self):
        """Close the serial connection."""
        self.serial.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def main():
    """Example usage."""
    import argparse

    parser = argparse.ArgumentParser(description="ScreenMetrics CLI")
    parser.add_argument("-p", "--port", help="Serial port (auto-detect if omitted)")
    parser.add_argument("-b", "--baudrate", type=int, default=9600,
                        help="Baud rate (default: 9600 for Arduino, use 115200 for ESP32/ESP8266)")
    parser.add_argument("-l", "--list-ports", action="store_true", help="List available ports")
    parser.add_argument("-s", "--set", nargs=2, metavar=("KEY", "VALUE"), help="Set a metric")
    parser.add_argument("-d", "--delete", metavar="KEY", help="Delete a metric")
    parser.add_argument("-L", "--list", action="store_true", help="List all metrics")
    parser.add_argument("-c", "--clear", action="store_true", help="Clear all metrics")
    args = parser.parse_args()

    if args.list_ports:
        ports = list_available_ports()
        if not ports:
            print("No serial ports found")
        else:
            for p in ports:
                print(f"{p['port']}: {p['description']}")
        return

    # Auto-detect port if not specified
    port = args.port
    if not port:
        port = find_port()
        if not port:
            print("Error: No serial port found. Specify with -p or connect a device.")
            return
        print(f"Auto-detected port: {port}")

    with ScreenMetrics(port, args.baudrate) as sm:
        if args.set:
            key, value = args.set
            print(sm.set(key, value))
        elif args.delete:
            print(sm.delete(args.delete))
        elif args.list:
            print(sm.list())
        elif args.clear:
            print(sm.clear())
        sm.close()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Python client for ScreenMetrics ESP8266 OLED display over WiFi."""

import argparse
import subprocess
import sys
import time
import urllib.request
import urllib.error
import socket


DEFAULT_AP_SSID = "ScreenMetrics"
DEFAULT_AP_GATEWAY = "192.168.4.1"
DEFAULT_AP_PASSWORD = "12345678"
TIMEOUT = 5


def wait_for_ap(ssid: str = DEFAULT_AP_SSID, timeout: int = 30) -> bool:
    """Wait for the ScreenMetrics AP to become available.

    Returns:
        True if AP is found, False otherwise.
    """
    print(f"Waiting for '{ssid}' AP...")
    start = time.time()
    while time.time() - start < timeout:
        try:
            # Try to ping the gateway to see if AP is up
            socket.create_connection((DEFAULT_AP_GATEWAY, 80), timeout=2)
            return True
        except (socket.timeout, OSError):
            time.sleep(1)
    return False


def connect_to_ap(ssid: str = DEFAULT_AP_SSID, password: str = DEFAULT_AP_PASSWORD) -> bool:
    """Connect to the ScreenMetrics AP.

    On Linux, uses nmcli. On macOS, uses networksetup.
    On Windows, uses netsh.

    Returns:
        True if connected successfully, False otherwise.
    """
    print(f"Connecting to WiFi network '{ssid}'...")

    system = subprocess.check_output(["uname", "-s"]).decode().strip()

    try:
        if system == "Linux":
            # Disconnect current connections
            subprocess.run(["nmcli", "dev", "wifi", "disconnect"], capture_output=True)
            time.sleep(1)
            # Connect to AP
            result = subprocess.run(
                ["nmcli", "dev", "wifi", "connect", ssid, "password", password],
                capture_output=True, text=True
            )
            if result.returncode == 0:
                print("Connected via nmcli")
                return True
            else:
                print(f"nmcli failed: {result.stderr}")
                return False

        elif system == "Darwin":  # macOS
            # Use networksetup to connect to WiFi
            networks = subprocess.check_output(
                ["networksetup", "-listpreferredwirelessnetworks", "en0"],
                text=True
            )
            if ssid in networks:
                result = subprocess.run(
                    ["networksetup", "-connectpreferredwirelessnetwork", "en0", ssid, password],
                    capture_output=True, text=True
                )
                if result.returncode == 0:
                    print("Connected via networksetup")
                    return True
                else:
                    print(f"networksetup failed: {result.stderr}")
                    return False
            else:
                print(f"Network '{ssid}' not found in preferred networks")
                return False

        elif system == "Windows":
            subprocess.run(["netsh", "wlan", "disconnect"], capture_output=True)
            time.sleep(1)
            result = subprocess.run(
                ["netsh", "wlan", "connect", f"name={ssid}", f"ssid={ssid}"],
                capture_output=True, text=True
            )
            if result.returncode == 0:
                print("Connected via netsh")
                return True
            else:
                print(f"netsh failed: {result.stderr}")
                return False

        else:
            print(f"Unknown system: {system}")
            return False

    except FileNotFoundError as e:
        print(f"Command not found: {e}")
        return False
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {e}")
        return False


def open_browser(url: str = f"http://{DEFAULT_AP_GATEWAY}"):
    """Open the default browser with the given URL."""
    print(f"Opening browser: {url}")
    try:
        subprocess.run(["xdg-open", url], capture_output=True)
    except FileNotFoundError:
        try:
            subprocess.run(["open", url], capture_output=True)
        except FileNotFoundError:
            try:
                subprocess.run(["start", "", url], shell=True, capture_output=True)
            except FileNotFoundError:
                print(f"Could not open browser. Please open: {url}")


def http_request(path: str, base_url: str = None) -> str:
    """Send HTTP GET request to the device."""
    if base_url is None:
        base_url = DEFAULT_AP_GATEWAY
    url = f"http://{base_url}{path}"

    try:
        with urllib.request.urlopen(url, timeout=TIMEOUT) as response:
            return response.read().decode('utf-8', errors='replace')
    except urllib.error.URLError as e:
        return f"ERROR: {e}"
    except socket.timeout:
        return "ERROR: Connection timed out"


class ScreenMetrics:
    """Send metrics to ScreenMetrics device via HTTP."""

    def __init__(self, host: str = DEFAULT_AP_GATEWAY):
        """Initialize HTTP connection.

        Args:
            host: IP address or hostname of the device
        """
        self.host = host
        self.base_url = f"http://{host}"

    def set(self, key: str, value: str) -> str:
        """Set or update a metric."""
        return http_request(f"/set?key={key}&value={value}", self.host)

    def list(self) -> str:
        """List all metrics."""
        return http_request("/list", self.host)

    def clear(self) -> str:
        """Clear all metrics."""
        return http_request("/clear", self.host)

    def status(self) -> str:
        """Get device status."""
        return http_request("/status", self.host)


def main():
    parser = argparse.ArgumentParser(description="ScreenMetrics CLI - WiFi edition")
    parser.add_argument("-H", "--host", default=DEFAULT_AP_GATEWAY,
                        help=f"Device IP address (default: {DEFAULT_AP_GATEWAY})")
    parser.add_argument("-s", "--set", nargs=2, metavar=("KEY", "VALUE"), help="Set a metric")
    parser.add_argument("-d", "--delete", metavar="KEY", help="Delete a metric")
    parser.add_argument("-L", "--list", action="store_true", help="List all metrics")
    parser.add_argument("-c", "--clear", action="store_true", help="Clear all metrics")
    parser.add_argument("-S", "--status", action="store_true", help="Show device status")
    parser.add_argument("--setup", action="store_true",
                        help="Connect to ScreenMetrics AP and open browser for WiFi setup")
    args = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help()
        return

    if args.setup:
        # Step 1: Wait for AP
        if wait_for_ap():
            print("AP found!")
            # Step 2: Connect to it
            if connect_to_ap():
                # Step 3: Open browser for captive portal
                open_browser()
            else:
                print("\nCould not connect to AP automatically.")
                print(f"Please connect to '{DEFAULT_AP_SSID}' WiFi network manually,")
                print(f"then open browser to: http://{DEFAULT_AP_GATEWAY}")
        else:
            print("\nAP not found. Is the ESP8266 running and in AP mode?")
            print(f"Please connect to '{DEFAULT_AP_SSID}' WiFi network manually,")
            print(f"password: {DEFAULT_AP_PASSWORD},")
            print(f"then open browser to: http://{DEFAULT_AP_GATEWAY}")
        return

    # Try to infer host from current network
    host = args.host
    if host == DEFAULT_AP_GATEWAY:
        # Check if we can reach the device at default gateway
        try:
            socket.create_connection((DEFAULT_AP_GATEWAY, 80), timeout=2)
        except (socket.timeout, OSError):
            # Can't reach default gateway, device might have different IP
            # Try to find it via mDNS
            try:
                import urllib.request
                socket.getaddrinfo("screenmetrics.local", 80)
                host = "screenmetrics.local"
            except socket.gaierror:
                pass

    sm = ScreenMetrics(host)

    if args.set:
        key, value = args.set
        print(sm.set(key, value))
    elif args.delete:
        print(sm.delete(args.delete))
    elif args.list:
        print(sm.list())
    elif args.clear:
        print(sm.clear())
    elif args.status:
        print(sm.status())
    else:
        # Show status by default
        print(sm.status())


if __name__ == "__main__":
    main()

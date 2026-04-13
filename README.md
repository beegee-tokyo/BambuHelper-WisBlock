# BambuHelper

----

_**This fork is for WisBlock RAK3312 ESP32-S3 with RAK14014 240x320 px TFT touch screen display.**_     
_**It is updated frequentely with updates made on the original code provided by @keralots**_     

----

Dedicated Bambu Lab printer monitor built with RAK3312 ESP32-S3 and the RAK14014 2.4" 240x320 color TFT display (ST7789) with FT6336U Touch Screen.

Connects to your printer via MQTT over TLS and displays a real-time dashboard with arc gauges, animations, live stats, and optional buzzer notifications.

For additional supported boards, please check [@Keralots BambuHelper repo](https://github.com/Keralots/BambuHelper).

### Supported Printers

| Connection Mode | Printers | How it connects |
|---|---|---|
| **LAN Direct** | P1P, P1S, X1, X1C, X1E, A1, A1 Mini | Local MQTT via printer IP + LAN access code |
| **LAN Direct (Developer Mode)** | H2S, H2C, H2D | LAN-only mode + Developer Mode required - see note below |
| **Bambu Cloud (All printers)** | Any Bambu printer | Cloud MQTT via access token - no LAN mode needed |

> **H2 series LAN mode:** H2S, H2C, and H2D printers require both **LAN-only mode** and **Developer Mode** enabled in printer settings for local MQTT to work. Without Developer Mode, the printer accepts connections but does not respond to status requests. If you prefer not to enable Developer Mode, use Bambu Cloud mode instead.

> **Tip:** Use "Bambu Cloud (All printers)" if you don't want to enable LAN/Developer mode on your printer (for example to keep Bambu Handy working), if your ESP32 is on a different network than the printer, or if your printer only supports cloud mode (P2S).

### Cloud Mode Security Notice

When using Bambu Cloud, BambuHelper connects through Bambu Lab's cloud MQTT service. Here is what you need to know:

- **No credentials are stored** - BambuHelper never asks for your email or password. You extract an access token from your browser and paste it into the web interface.
- **Only the access token is stored** in the ESP32's flash memory. This token expires after about 3 months, at which point you simply paste a new one.
- **Read-only access** - BambuHelper only reads printer status. It never sends commands or modifies printer settings.
- **Same approach as other community projects** - this is the same authentication method used by the [Home Assistant Bambu Lab integration](https://github.com/greghesp/ha-bambulab), [OctoPrint-Bambu](https://github.com/jneilliii/OctoPrint-BambuPrinter), and other trusted third-party tools.

## Screenshots

| Dashboard | Web Interface - Settings | Web Interface - Gauge Colors |
|---|---|---|
| <img src="img/RAK3312-RAK14014.png" width="900"> | ![Settings](img/screen1.png) | ![Gauge Colors](img/screen2.png) |

## Supported Boards

| Preview | Board | Notes |
|--------------------------------------|-------|-------|
| <img src="img/RAK3312-RAK14014.png" width="900"> | **WisBlock RAK3312 ESP32-S3 + RAK14014 Touch LCD** | `240x320` ST7789 version with ESP32-S3, built with WisBlock Modular System. No soldering required, all components are just plugged together. Use the `rak3312` firmware build. Supports **up to 2 printers**, like the main ESP32-S3 DIY version. See [WisBlock Modular System components](#wisblock-modular-system-components) for used modules. Matching enclosure on [MakerWorld](https://makerworld.com/en/models/2571194-wisblock-bambuhelper-landscape) and [Instructables](https://www.printables.com/model/1649739-wisblock-bambuhelper-landscape) |

Check [@Keralots BambuHelper repo](https://github.com/Keralots/BambuHelper) for other supported MCU/Display options

## Features

- **Live dashboard** - progress arc, temperature gauges, fan speed, layer count, time remaining
- **H2-style LED progress bar** - full-width glowing bar inspired by Bambu H2 series
- **Anti-aliased arc gauges** - smooth nozzle and bed temperature arcs with color zones
- **Animations** - loading spinner, progress pulse, completion celebration
- **Web config portal** - dark-themed settings page for WiFi, network, printer, display, power, and buzzer settings
- **Network configuration** - DHCP or static IP, with optional IP display at startup
- **Display auto-off** - configurable timeout after print completion, auto-off when printer is off
- **NVS persistence** - all settings survive reboots
- **Auto AP mode** - creates WiFi hotspot on first boot or when WiFi is lost
- **Smart redraw** - only redraws changed UI elements for smooth performance
- **Customizable gauge colors** - per-gauge arc/label/value colors with preset themes
- **Multi-printer support** - monitor up to 2 printers simultaneously with auto-rotating display (ESP32-S3 only - CYD/C3 limited to 1 printer due to RAM)
- **Smart rotation** - automatically shows the printing printer; cycles between both when both are printing
- **Physical button** - optional push button or TTP223 touch sensor to cycle printers and wake display
- **Optional buzzer** - passive buzzer notifications for print finished, connected, and error events
- **OTA updates** - firmware can be updated from the web interface
- **Additional board support** - CYD 240x320, Waveshare 2" 240x320, and ESP32-C3 240x240 builds are supported
- **Exponential backoff** - reconnect attempts to offline printers gradually slow down to conserve resources

## Hardware

### Default Wiring

> **Note:** WisBlock RAK3312" is a modular concepts with the display already integrated. No additional wiring or soldering is required.     

### Optional Touch Sensor / Button Wiring

> **Note:** For WisBlock RAK3312 with RAK14014 TFT display select **TouchScreen** in the web interface.

### Optional Buzzer Wiring

Optional a [WisBlock RAK18001 Buzzer](https://store.rakwireless.com/products/wisblock-buzzer-module-rak18001) can be added in Sensor Slot A of the WisBlock Base Board.     

> **Note:** For WisBlock RAK3312 with RAK14014 TFT display and the RAK18001 buzzer, the GPIO pin is `GPIO 21`.

> **WARNING:** Due to the high current peak of the buzzer, it requires to use a battery connected to the WisBlock Base Board. Otherwise the ESP32-S3 might trigger a power failure and reboots.

### Assembly Instructions

[Assembly Guide](./img/WisBlock BambuHelper assembly guide for landscape enclosure version.pdf)

### WisBlock Modular System components

| Component Wisblock | Specification |
|---|---|
| MCU | [RAK3312](https://store.rakwireless.com/products/wisblock-core-module-rak3312-lora-wifi-ble) ESP32-S3 |
| Base Board | [RAK19007](https://store.rakwireless.com/products/rak19007-wisblock-base-board-2nd-gen) Base Board |
| Display | [RAK14014](https://store.rakwireless.com/products/240x320-pixel-full-color-tft-display-with-touch-screen-rak14014) 320x240 2.4" TFT Touch Screen display |
| Buzzer | [RAK18001](https://store.rakwireless.com/products/wisblock-buzzer-module-rak18001) Buzzer |

## Flashing

1. Download the latest firmware from [Releases](../../releases). **If you are flashing a new device for the first time**, use the file ending with **-Full** (for example `BambuHelper-esp32s3-v2.7-Full.bin`). The regular `-ota.bin` file is for OTA updates on devices that already have BambuHelper installed.
2. Open [ESP Web Flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge
3. If you are flashing a **CYD**, set **Baudrate** to **115200** before clicking **Connect**. Two or more attempts may be needed - the first one will fail. This applies to **CYD only**.
4. Connect your ESP32 via USB
5. Click **Connect** and select your device
6. Set flash address to **0x0**
7. Select the downloaded `.bin` file
8. Click **Program**

### Build Files

| Board | Use this `Full` file for first flash / recovery |
|---|---|
| WisBlock RAK3312 | `BambuHelper-rak3312-vx.y-Full.bin` |
| Other ESP32 Boards | Check [@Keralots BambuHelper repo](https://github.com/Keralots/BambuHelper) |

## Setup

### Configuration Guide

[![Configuration Guide](https://img.youtube.com/vi/n2RdbeHTMz0/maxresdefault.jpg)](https://youtu.be/n2RdbeHTMz0) 

1. **Flash** the firmware (see above)
2. **Connect** to the `BambuHelper-XXXX` WiFi network (password: `bambu1234`)
3. **Open** `192.168.4.1` in your browser
4. **Enter** your home WiFi credentials and **Save** - the device restarts and connects to your WiFi
5. **Note the IP address** shown on the ESP32 display after it connects to WiFi
6. **Open** that IP address in your browser to access the full web interface
7. **Configure your printer:**

   **LAN Direct** (P1P, P1S, X1, X1C, X1E, A1, A1 Mini):
   - Printer IP address (found in printer Settings > Network)
   - Serial number (see note below)
   - LAN access code (8 characters, from printer Settings > Network)

   **Bambu Cloud (All printers)**:
   - Get your Bambu Cloud access token from your browser (see [Getting a Cloud Token](#getting-a-cloud-token) below)
   - Paste the token into the web interface
   - Enter your printer's serial number (see note below)

   > **Important: Serial number is NOT the printer name.** The serial number is a 15-character code (for example `01P00A000000000`) found on the printer LCD under **Settings > Device > Serial Number**, or on the physical label on the back or bottom of the printer. Do not confuse it with the printer name shown in Bambu Studio (for example `3DP-01P-110`), which is a shortened version and will not work.

8. **Save Printer Settings** - the device connects to your printer

### Getting a Cloud Token

To use cloud mode, you need an access token from your Bambu Lab account. There are two ways to get it:

**Using browser DevTools (Chrome / Edge):**
1. Open https://bambulab.com and log in to your account
2. Press **F12** to open DevTools
3. Go to the **Application** tab (click `>>` if you do not see it)
4. In the left sidebar, expand **Cookies** -> click `https://bambulab.com`
5. Find the row named `token` in the cookie list
6. Double-click the **Value** cell to select it, then **Ctrl+C** to copy
7. Paste the value into BambuHelper's "Access Token" field in the web interface

**Using browser DevTools (Firefox):**
1. Open https://bambulab.com and log in to your account
2. Press **F12** to open DevTools
3. Go to the **Storage** tab
4. In the left sidebar, expand **Cookies** -> click `https://bambulab.com`
5. Find the row named `token`
6. Double-click the **Value** cell to select it, then **Ctrl+C** to copy
7. Paste the value into BambuHelper's "Access Token" field

**Using browser DevTools (Safari):**
1. Open https://bambulab.com and log in to your account
2. Open **Develop** -> **Show Web Inspector** (enable the Develop menu first in Safari Preferences -> Advanced)
3. Go to the **Storage** tab -> **Cookies** -> `bambulab.com`
4. Find and copy the `token` value
5. Paste it into BambuHelper's "Access Token" field

**Using the Python helper script (recommended):**
```bash
pip install curl_cffi
python tools/get_token.py
```
The script will prompt for your email, password, and 2FA code, then print the token. Copy and paste it into BambuHelper's web interface.

> **Note:** The token is valid for approximately 3 months. When it expires, the ESP32 will fail to connect - simply repeat the process above to get a fresh token and paste it in the web interface. Make sure to select the correct **Server Region** (US/EU/CN) to match your Bambu account's region.

## Web Interface

The built-in web interface (accessible at the device's IP address) provides the following settings:

### WiFi Settings
- **SSID** - your home WiFi network name
- **Password** - WiFi password

### Network
- **IP Assignment** - choose between DHCP (automatic) or Static IP
- **Static IP fields** (when static is selected):
  - IP Address
  - Gateway
  - Subnet Mask
  - DNS Server
- **Show IP at startup** - display the assigned IP on screen for 1.5 seconds after WiFi connects (on by default)

### Printer Settings
- **Connection Mode** - LAN Direct or Bambu Cloud (All printers)
- **LAN mode fields:**
  - Printer Name, Printer IP Address, Serial Number, LAN Access Code
- **Cloud mode fields:**
  - Server Region (US/EU/CN), Access Token, Printer Serial Number, Printer Name
- **Live Stats** - real-time nozzle/bed temp, progress, fan speed, and connection status

### Display
- **Brightness** - backlight level (10-255)
- **Screen Rotation** - 0, 90, 180, 270 degrees
- **Display off after print complete** - minutes to show the finish screen before turning off the display (0 = never turn off, default: 3 minutes)
- **Keep display always on** - override the timeout and never turn off
- **Show clock after print** - display a digital clock with date instead of turning off the screen (enabled by default)

### Gauge Colors
- **Theme presets** - Default, Mono Green, Neon, Warm, Ocean
- **Background color** - display background
- **Track color** - inactive arc background
- **Per-gauge colors** (arc, label, value) for:
  - Progress
  - Nozzle temperature
  - Bed temperature
  - Part fan
  - Aux fan
  - Chamber fan

### Buzzer
- **Buzzer (optional)** - enable or disable passive buzzer notifications
- **GPIO Pin** - choose which ESP32 pin drives the buzzer (GPIO21 for WisBlock RAK18001 in Slot A)
- **Quiet Hours** - disable buzzer sounds during selected hours
- **Test Buttons** - quickly test available buzzer sounds from the web interface

### Other
- **Factory Reset** - erases all settings and restarts
- **OTA Update** - update firmware directly from the web interface

## Dashboard Screens

| Screen | When |
|---|---|
| Splash | Boot (2 seconds) |
| AP Mode | First boot / no WiFi configured |
| Connecting WiFi | Attempting WiFi connection |
| WiFi Connected | Shows IP for 1.5 seconds (if enabled) |
| Connecting Printer | WiFi connected, waiting for MQTT |
| Idle | Connected, printer not printing |
| Printing | Active print with full dashboard |
| Finished | Print complete with animation (auto-off after timeout) |
| Clock | After finish timeout (if enabled) - shows digital clock with date |
| Display Off | After finish timeout (if clock disabled) or printer powered off |

## Display Power Management

- After a print completes, the finish screen is shown for a configurable duration (default: 3 minutes), then either a digital clock is displayed or the screen turns off (configurable).
- When the printer is powered off or disconnected, the display stays in its current state (clock or off).
- When the printer comes back online or starts a new print, the display automatically wakes up.
- The "Keep display always on" option overrides the auto-off behavior.
- The "Show clock after print" option (enabled by default) shows time and date instead of turning off the display.

## Project Structure

```
include/
  config.h              Pin definitions, colors, constants
  bambu_state.h         Data structures (BambuState, PrinterConfig, ConnMode)
src/
  main.cpp              Setup/loop orchestrator
  settings.cpp          NVS persistence (WiFi, network, printer, display, power, cloud token)
  wifi_manager.cpp      WiFi STA + AP fallback, static IP support
  web_server.cpp        Config portal (HTML embedded, token management)
  bambu_mqtt.cpp        MQTT over TLS, delta merge (local + cloud broker)
  bambu_cloud.cpp       Bambu Cloud helpers (region URLs, JWT userId extraction)
  button.cpp            Physical button / touch sensor input
  buzzer.cpp            Optional passive buzzer support
  display_ui.cpp        Screen state machine
  display_gauges.cpp    Arc gauges, progress bar, temp gauges
  display_anim.cpp      Animations (spinner, pulse, dots)
  clock_mode.cpp        Digital clock display (after print finishes)
  icons.h               16x16 pixel-art icons
tools/
  get_token.py          Python helper to get Bambu Cloud token on PC
```

## Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- **LAN mode:** Bambu Lab printer with LAN mode enabled, printer and ESP32 on the same local network
- **Cloud mode:** Bambu Lab account, ESP32 with internet access

## Multi-Printer Monitoring

BambuHelper supports monitoring up to 2 printers simultaneously via dual MQTT connections.

> **CYD and ESP32-C3 boards are limited to 1 printer.** Each MQTT connection requires ~85 KB of RAM (TLS session + message buffer), and the classic ESP32 / C3 do not have enough free heap for two simultaneous connections. The web interface on these boards hides the second printer tab and shows a notice. Use an ESP32-S3 board if you need two printers.

### Rotation Modes

| Mode | Behavior |
|---|---|
| **Smart** (default) | Shows the printing printer. If both are printing, cycles between them. If neither is printing, shows last active. |
| **Auto-rotate** | Cycles through all connected printers at a configurable interval (10s - 10min). |
| **Off** | Manually switch between printers using the physical button only. |

### Physical Button

The version with RAK3312 and RAK14014 TFT display does not require a physical button, as it has a touch screen.      
Select **Touchscreen** in the WebUI in _Hardware & Multi-Printer_ section.

### MQTT Reconnect Backoff

When a printer is physically powered off, BambuHelper uses exponential backoff to avoid wasting resources on repeated connection attempts:

| Phase | Attempts | Interval |
|---|---|---|
| Normal | First 5 | Every 10 seconds |
| Phase 2 | Next 10 | Every 60 seconds |
| Phase 3 | Beyond 15 | Every 120 seconds |

When the printer comes back online, the backoff resets to normal immediately.

## Power Monitoring

| | |
|---|---|
| ![Power Monitoring](img/PowerMonitoring.png) | BambuHelper can display live power consumption from a **[Tasmota](https://tasmota.github.io/docs/)-flashed smart plug** connected to your printer. Tasmota is open-source firmware for ESP-based smart plugs that exposes a local HTTP API and MQTT - no cloud required.<br><br>**What it shows:**<br>- Live wattage in the bottom status bar on the idle and printing screens<br>- Total kWh used during the print job, shown on the "Print Complete" screen<br><br>**Setup:** open the web interface, go to **Power Monitoring**, enter the plug's local IP address, set your preferred poll interval (10-30s), and choose whether to alternate the watts display with the layer counter or always show watts.<br><br>**Requirements:** any Tasmota-flashed smart plug with energy monitoring (e.g. Sonoff S31, BlitzWolf BW-SHP6, Nous A1). The plug must be on your local network and reachable from the ESP32. No Tasmota MQTT broker needed - BambuHelper polls the HTTP API directly.<br><br>Future plans include automatic printer power-off based on nozzle temperature and idle time. |

## Troubleshooting

### WiFi won't connect / drops frequently

**Antenna is not connected to MHF-4 connector of the RAK3312**    
Make sure the WiFi antenna that is coming with the RAK3312 is connected to the correct antenna connector on the RAK3312.e ESP32 module.

**Symptoms:**
- "Connecting to WiFi" screen appears briefly, then falls back to AP mode
- WiFi connects sometimes but drops after a few seconds
- Works fine when display is disconnected

### Printer shows "Connecting" but never connects

- **LAN Direct:** Make sure the printer and ESP32 are on the same network. Check that LAN mode is enabled on the printer and the access code is correct.
- **Bambu Cloud:** Verify the access token has not expired (about 3 months validity). Re-extract it from your browser and paste it again. Check the server region matches your Bambu account.
- If a printer is physically powered off, reconnect attempts will gradually slow down (backoff). It will reconnect automatically when the printer comes back online.

### Display shows wrong printer / does not switch

- Check rotation mode in the web interface (Multi-Printer section). Smart mode only switches automatically when a printer is actively printing.
- Press the physical button (if configured) to manually cycle between printers.

## Future Plans

- Keep updated with new features added by @keralots

## License

MIT

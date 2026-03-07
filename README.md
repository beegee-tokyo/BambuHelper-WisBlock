# BambuHelper

Dedicated Bambu Lab P1S printer monitor built with ESP32-S3 Super Mini and a 1.54" 240x240 color TFT display (ST7789).

Connects to your printer via MQTT over TLS and displays a real-time dashboard with arc gauges, animations, and live stats.

## Features

- **Live dashboard** — progress arc, temperature gauges, fan speed, layer count, time remaining
- **H2-style LED progress bar** — full-width glowing bar inspired by Bambu H2 series
- **Anti-aliased arc gauges** — smooth nozzle and bed temperature arcs with color zones
- **Animations** — loading spinner, progress pulse, completion celebration
- **Web config portal** — dark-themed settings page for WiFi and printer credentials
- **NVS persistence** — settings survive reboots
- **Auto AP mode** — creates WiFi hotspot on first boot or when WiFi is lost
- **Smart redraw** — only redraws changed UI elements for smooth performance

## Hardware

| Component | Specification |
|---|---|
| MCU | ESP32-S3 Super Mini |
| Display | 1.54" TFT SPI ST7789 (240x240) |
| Connection | SPI |

### Default Wiring

| Display Pin | ESP32-S3 GPIO |
|---|---|
| MOSI (SDA) | 11 |
| SCLK (SCL) | 12 |
| CS | 10 |
| DC | 8 |
| RST | 9 |
| BL | 7 |

Adjust pin assignments in `platformio.ini` build_flags to match your wiring.

## Setup

1. **Flash** the firmware via PlatformIO (`pio run -t upload`)
2. **Connect** to the `BambuHelper-XXXX` WiFi network (password: `bambu1234`)
3. **Open** `192.168.4.1` in your browser
4. **Enter** your home WiFi credentials and printer details:
   - Printer IP address (found in printer Settings > Network)
   - Serial number
   - LAN access code (8 characters, from printer Settings > Network)
5. **Save** — the device restarts and connects to your printer

## Dashboard Screens

| Screen | When |
|---|---|
| AP Mode | First boot / no WiFi configured |
| Connecting WiFi | Attempting WiFi connection |
| Connecting Printer | WiFi connected, waiting for MQTT |
| Idle | Connected, printer not printing |
| Printing | Active print with full dashboard |
| Finished | Print complete with animation |

## Project Structure

```
include/
  config.h              Pin definitions, colors, constants
  bambu_state.h         Data structures (BambuState, PrinterConfig)
src/
  main.cpp              Setup/loop orchestrator
  settings.cpp          NVS persistence
  wifi_manager.cpp      WiFi STA + AP fallback
  web_server.cpp        Config portal (HTML embedded)
  bambu_mqtt.cpp        MQTT over TLS, delta merge
  display_ui.cpp        Screen state machine
  display_gauges.cpp    Arc gauges, progress bar, temp gauges
  display_anim.cpp      Animations (spinner, pulse, dots)
  icons.h               16x16 pixel-art icons
```

## Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Bambu Lab printer with LAN mode enabled
- Printer and ESP32 on the same local network

## Future Plans

- Multi-printer monitoring (up to 4 printers)
- Physical buttons for switching between printers
- OTA firmware updates
- Additional printer model support (X1C, A1, P1P)

## License

MIT

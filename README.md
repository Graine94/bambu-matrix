# Bambu Matrix

**English** · [Русский](README.ru.md)

Bambu Lab 3D printer status on a MAX7219 64×8 LED matrix driven by an ESP32.
Progress, time left, heating, pause, bed cooldown — and a clock with weather while idle.
Everything is configured from a phone in the web UI (English or Russian), updates go over Wi-Fi.

**Install from the browser:** https://graine94.github.io/bambu-matrix/ (Chrome or Edge on a computer)

## What you need

| | |
|---|---|
| Board | any classic **ESP32** with 4 MB flash: ESP32 DevKit, LOLIN32 Lite, WROOM-32, etc. (ESP32-S2/S3/C3 won't work) |
| Matrix | MAX7219, **8 modules of 8×8** in a row (64×8), usually sold as two “FC-16 4-in-1” or one “8-in-1” |
| Printer | Bambu Lab: X1, P1, A1, P2S, H2D — on the same network as the matrix |
| Power | 5 V for the matrix, common ground with the ESP32 |

### Wiring

| MAX7219 | ESP32 |
|---|---|
| VCC | 5 V (VIN / 5V) |
| GND | GND |
| DIN | GPIO23 |
| CLK | GPIO18 |
| CS | GPIO5 |

If the board has no 5 V pin (like the LOLIN32 Lite), power the matrix from a separate 5 V USB supply
and connect the matrix GND to the board GND. Never feed 5 V into the 3V3 pin.

## Installing

### From the browser (easiest)

1. Open https://graine94.github.io/bambu-matrix/ in Chrome or Edge on a computer.
2. Connect the ESP32 over USB, press “Install on ESP32” and pick the port.

### From source (PlatformIO)

```bash
pio run -t upload
```

## First setup

1. After flashing, the matrix scrolls `SETUP WIFI: BambuMatrix PASS: bambu123`.
2. On your phone, join the **BambuMatrix** Wi-Fi (password `bambu123`). The setup page opens
   (or go to `192.168.4.1`). “Configure WiFi” → your 2.4 GHz network → password → Save.
   The printer fields can stay empty.
3. The matrix joins your network and scrolls its address, for example `IP 192.168.1.53`.
4. Open that address (or `http://bambu-matrix.local`). The page shows only the printer connection step:
   tap your printer under “Found on the network” and enter its **Access Code** — it's on the printer
   screen in the network (WLAN) settings. Once connected, “Open settings” takes you to everything else.

The printer's serial number is detected automatically. If some model doesn't provide it, enter it manually in the web UI.
The web UI language can be switched with the EN / RU buttons at the top; the choice is stored on the matrix.

## Messages on the matrix

| Matrix | Meaning |
|---|---|
| `SETUP WIFI: BambuMatrix …` | no Wi-Fi — join BambuMatrix and set up the network |
| `WiFi...` | looking for the saved network (after 3 minutes without it, SETUP comes back) |
| `IP 192.168.1.53` | connected to Wi-Fi, this is the web UI address |
| `SETUP: http://…` | no printer selected — open the address and pick the printer |
| `ACCESS CODE?` | the printer rejected the Access Code |
| `PRINTER … NOT FOUND` | nothing answers at this IP (check the IP and that the printer is on) |
| `OTA` and a bar | firmware update over Wi-Fi |

If the matrix has connected with these settings at least once and the printer disappears later, it has
simply been turned off: the display goes dark (or shows the clock, if you chose that).

## Web UI features

- 7 progress styles with previews and a demo on the matrix
- day and night brightness: on a schedule or from sunset to sunrise
- smooth on/off; a picture check for differently wired modules and 180° rotation
- nozzle and bed heating before the print, 3 pause modes
- after the print: animation, bed cooldown to READY, display auto-off
- while idle: clock, clock with weather (Open-Meteo, or MET Norway if Open-Meteo is unreachable; no key), “rain” screensaver
- weather icons for day and night: clear, partly cloudy, cloudy, fog, drizzle, rain, snow, thunderstorm
- automatic time zone from the city
- printer discovery on the network, clear connection status
- English and Russian interface
- update check and one-click install from GitHub, on demand

## Updates

The firmware version is shown under “System” in the web UI.

- **From GitHub.** Press **Check for updates** under “System”. If there is a newer release, the web UI
  shows its version and release notes and an **Install update** button: the matrix downloads the firmware from GitHub itself, shows `OTA` and a bar,
  and restarts. All settings, including Wi-Fi, are kept. The download is accepted only from GitHub —
  the connection is checked against GitHub's root certificates (`include/github_ca.h`).
- **From a file.** “System” → “Update firmware” → `bambu-matrix-ota.bin` from a
  [release](https://github.com/Graine94/bambu-matrix/releases).
- **Reinstalling from the browser installer** keeps Wi-Fi and settings too, unless you tick
  “Erase device” in the installer.
- **With esptool**, everything in one file (this wipes Wi-Fi and settings):
  `esptool --chip esp32 write-flash 0x0 bambu-matrix.bin`
- **From source:** `pio run -e ota -t upload` (to `bambu-matrix.local`).

## Troubleshooting

- **The printer isn't in the list** — make sure the printer and the matrix are on the same network
  (guest networks and separate VLANs get in the way). Enter the IP manually.
- **`ACCESS CODE?`** — the code may have changed: check the current one on the printer screen.
- **Connects but no status** — on recent firmware, some models require “LAN Only” or “Developer Mode”
  on the printer for third-party devices.
- **P1/A1 with many clients** — these models limit the number of simultaneous local connections
  (Bambu Studio, Handy, Home Assistant). Close the extra ones.
- **Mirrored or garbled picture** — “System” → “Picture looks wrong?”: try the variants until the matrix reads `ABC 123` left to right.
- **Won't connect to Wi-Fi** — the ESP32 only supports 2.4 GHz.
- **All LEDs light up at power-on and stay lit** — the MAX7219 chips came up before the ESP32 and
  went into test mode. The firmware resends the matrix settings every second, so this clears by itself.
  If it doesn't, add a 10 kΩ pull-up from CS to 3.3 V and a 100–470 µF capacitor across the matrix 5 V and GND.

## Security

The web UI and firmware updates are open to everyone on the local network, without a password. The setup
access point password is `bambu123` (set in `src/main.cpp`, `AP_PASS`). Don't make the matrix reachable
from the internet (no port forwarding to it on the router).

The matrix itself only makes outgoing connections, and only to these services:

- `pool.ntp.org`, `time.google.com` — time
- `api.open-meteo.com`, and `api.met.no` as a fallback — weather, sunrise and sunset
- `api.github.com`, `github.com` and its file storage — only when you press “Check for updates” or “Install update”

The city search in the web UI (`geocoding-api.open-meteo.com`) runs in your phone's browser, not on the matrix.
The printer is reached only over the local network; nothing about it is sent to the internet.

## How it works

The ESP32 connects to the printer over local MQTT (TLS, port 8883, user `bblp`, password = Access Code)
and subscribes to `device/<serial>/report`. Reports are parsed as a stream with a filter, so large messages
never sit in memory whole. Matrix rendering runs in its own task on core 0, networking and logic on core 1.
Printers on the network are discovered over SSDP, the same way Bambu Studio finds them.

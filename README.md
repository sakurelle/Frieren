# Frieren

`Frieren` is an ESP32-C3 PlatformIO + ESP-IDF project for controlling a 5 V LED strip through an IRLZ44N low-side MOSFET, with a local Wi-Fi web interface for brightness and effect control.

## Hardware

- Board: ESP32-C3 Super Mini
- Framework: ESP-IDF via PlatformIO
- Strip power: 5 V boost converter output
- Switch element: IRLZ44N on the strip ground line
- Battery: 18650 through TP4056 and a 3.3 V regulator for ESP32
- Single external Type-C port is used either for charging detection or for the physical key

## GPIO wiring

- `GPIO5` -> Type-C key input
  Type-C `D+` goes to `GPIO5` through `4.7 kOhm`
  `GPIO5` is externally pulled up to `3.3 V` through `100 kOhm`
  Type-C `G` goes to common `GND`
  In the key plug, `D+` is shorted to `G`
  If the key is inserted, `GPIO5 = LOW`
- `GPIO7` -> USB 5 V presence detect
  Type-C `5V` goes through a divider `100 kOhm / 150 kOhm`
  Divider midpoint goes to `GPIO7`
  If USB 5 V is present, `GPIO7 = HIGH`
- `GPIO6` -> IRLZ44N gate control with PWM
  `GPIO6` goes to Gate through `220 Ohm`
  Gate is pulled down to `GND` through `100 kOhm`
  Source goes to common `GND`
  Drain goes to LED strip minus
  LED strip plus goes to `+5 V` from the boost converter

## Project structure

```text
Frieren/
  platformio.ini
  README.md
  src/
    CMakeLists.txt
    main.c
    app_config.h
    app_state.h
    app_state.c
    gpio_hw.h
    gpio_hw.c
    light_pwm.h
    light_pwm.c
    effects.h
    effects.c
    wifi_ap.h
    wifi_ap.c
    web_server.h
    web_server.c
    index.html.S
  web/
    index.html
```

`web/index.html` is the editable source of the web UI. Before build, PlatformIO regenerates `src/index.html.S` with the ESP-IDF embed script, and the HTTP server serves it through `_binary_index_html_*` symbols.

## Device modes

- `MODE_CHARGE`
  USB 5 V is present
  LED strip is forced off
- `MODE_KEY_ACTIVE`
  USB 5 V is absent and the Type-C key is inserted
  LED strip runs with the selected brightness and effect
- `MODE_IDLE`
  USB 5 V is absent and the Type-C key is not inserted
  LED strip is off

## Effects

- `Static`
- `Breath`
- `Dragon Breath`
- `Candle`
- `Fire Flicker`
- `Strobe`
- `Pulse`
- `Heartbeat`
- `Fade In/Out`

## Wi-Fi and web UI

- SoftAP SSID: `Frieren`
- Password: none
- Web UI: [http://192.168.1.4/](http://192.168.1.4/)
- HTTP API:
  `GET /api/status`
  `GET /api/set?brightness=70&effect=breath`

Returned JSON:

```json
{
  "mode": "CHARGE",
  "usb_present": true,
  "key_inserted": false,
  "light_enabled": false,
  "brightness": 70,
  "effect": "breath"
}
```

Brightness and effect are stored in NVS, so they survive reboot.

## Build and flash

Build:

```powershell
pio run
```

Flash:

```powershell
pio run -t upload
```

Serial monitor:

```powershell
pio device monitor
```

The project uses:

- board: `esp32-c3-devkitm-1`
- framework: `espidf`
- monitor speed: `115200`
- upload speed: `460800`

## Notes

- The firmware intentionally stays awake at this stage to keep flashing and serial debugging simple.
- Deep sleep is intentionally not implemented yet and will be added in a separate step.

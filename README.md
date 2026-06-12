# Frieren

`Frieren` is an ESP32-C3 PlatformIO + ESP-IDF project for controlling a 5 V LED strip from a local Wi-Fi web interface. The firmware detects charging vs. key insertion, enables the SX1308 boost converter only when needed, and drives strip brightness/effects with LEDC PWM on the IRLZ44N gate.

## Hardware

- Board: ESP32-C3 Super Mini
- Framework: ESP-IDF via PlatformIO
- Battery: 18650 through TP4056 and a 3.3 V regulator for ESP32
- Boost converter: SX1308 with dedicated `EN` control
- Strip switching: IRLZ44N on the LED strip ground line after the boost converter
- A single external Type-C port is used either for charging detection or for the physical key

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
- `GPIO8` -> `EN` of SX1308
  `GPIO8` goes to `EN` through `1 kOhm`
  `EN` is pulled down to `GND` through `100 kOhm`
  `GPIO8 = HIGH` enables the boost converter
  `GPIO8 = LOW` disables the boost converter
- `GPIO6` -> IRLZ44N gate PWM for the LED strip
  `GPIO6` goes to Gate through `220 Ohm`
  Gate is pulled down to `GND` through `100 kOhm`
  Source goes to common `GND`
  Drain goes to LED strip minus
  SX1308 `OUT+` goes to LED strip plus
  IRLZ44N controls the strip low side after the boost converter

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
  web/
    index.html
```

`web/index.html` is the editable source of the web UI. Before build, PlatformIO regenerates `src/index.html.S`, and the HTTP server serves the embedded page through `_binary_index_html_*` symbols.

## Device modes

- `MODE_CHARGE`
  USB 5 V is present
  Boost converter is off
  LED strip PWM is `0`
- `MODE_KEY_ACTIVE`
  USB 5 V is absent and the Type-C key is inserted
  Boost converter is enabled through `GPIO8`
  After a short startup delay, the strip runs with the selected brightness and effect through PWM on `GPIO6`
- `MODE_IDLE`
  USB 5 V is absent and the Type-C key is not inserted
  Boost converter is off
  LED strip PWM is `0`

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
- Password: `Frieren1`
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
  "pwm_available": true,
  "hardware_mode": "boost_en_and_led_pwm",
  "boost_enabled": false,
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
- `GPIO8` on ESP32-C3 may be a strapping pin. If the board starts booting or flashing unreliably with SX1308 `EN` on `GPIO8`, move `EN` to another free GPIO such as `GPIO10` and update `APP_GPIO_BOOST_EN`.

# Frieren

`Frieren` is an ESP32-C3 PlatformIO + ESP-IDF project for controlling a battery-powered LED strip from a local Wi-Fi web interface. The strip is powered directly from the battery path (`TP4056 OUT+ / OUT-`), while ESP32-C3 drives the IRLZ44N gate with LEDC PWM for brightness and slow, smooth lighting effects.

## Hardware

- Board: ESP32-C3 Super Mini
- Framework: ESP-IDF via PlatformIO
- Battery: 18650 through TP4056 and a 3.3 V regulator for ESP32
- LED strip power: directly from `TP4056 OUT+ / OUT-`
- Strip switching: IRLZ44N on the LED strip ground line
- A single external Type-C port is used either for charging detection or for the physical key

Because the strip is powered directly from the battery, its maximum brightness depends on the current battery voltage.

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
- `GPIO6` -> IRLZ44N gate PWM for the LED strip
  `GPIO6` goes to Gate through `220 Ohm`
  Gate is pulled down to `GND` through `100 kOhm`
  Source goes to `TP4056 OUT-` / common `GND`
  Drain goes to LED strip minus
  `TP4056 OUT+` goes to LED strip plus
- `GPIO8`
  Not used in the current hardware revision

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
  LED strip PWM is `0`
  The strip is forced off
  Web UI stays available while charging is connected
- `MODE_KEY_ACTIVE`
  USB 5 V is absent and the Type-C key is inserted
  The strip runs with the selected brightness and effect using PWM on `GPIO6`
- `MODE_IDLE`
  USB 5 V is absent and the Type-C key is not inserted
  LED strip PWM is `0`
  After `APP_SLEEP_DELAY_MS`, the device enters deep sleep

## Deep sleep

- Deep sleep is enabled in firmware by default
- The device goes to sleep when the key is removed and charging is not connected
- Before sleep, firmware sets PWM to `0`, stops the HTTP server and stops Wi-Fi
- The firmware logs raw `GPIO5` and `GPIO7` levels before entering sleep
- Wakeup source is only one:
  key insert -> `GPIO5 LOW`

Important notes:

- `GPIO5` must keep the external `100 kOhm` pull-up to `3.3 V` because wakeup is triggered by LOW level
- Charging through Type-C may not wake ESP32 from deep sleep, and this is expected
- TP4056 continues charging the battery fully in hardware without any ESP32 participation
- `GPIO7` is used only for charger detection after ESP32 is already awake
- If `GPIO5` is not valid for deep sleep wakeup on your exact board, firmware logs `KEY_SENSE GPIO is not valid for deep sleep wakeup; deep sleep disabled` and intentionally stays awake instead of sleeping

## Effects

- `Static`
  Constant light without smoothing delay
- `Breath`
  Very slow 8-10 second breathing wave
- `Dragon Breath`
  Long base cycle with soft rare surges
- `Candle`
  Calm, warm flicker with slow target changes
- `Fire Flicker`
  More active than candle, but still smooth and slow
- `Strobe`
  Reworked into a soft slow blink instead of hard flashing
- `Pulse`
  Long pulse with gentle attack and release
- `Heartbeat`
  Slower double-beat with softer peaks
- `Fade In/Out`
  Long smooth fade cycle

All non-static effects are additionally smoothed in firmware before they are written to PWM, so transitions on the strip stay softer.

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
  "brightness": 70,
  "effect": "breath",
  "hardware_mode": "battery_direct_led_pwm",
  "deep_sleep_enabled": true,
  "charger_wakeup_enabled": false
}
```

The web UI protects unsaved brightness/effect changes from being overwritten by background `/api/status` refreshes. Status cards keep updating every 2 seconds, but local form values stay untouched until the user applies them.

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

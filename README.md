# hausboot-spuelung
Managing the relays timings for a pump and a water solenoid valve for a marine toilet

Built on [ProjectUtils-esp32](https://github.com/Cameloah/ProjectUtils-esp32): WiFi setup,
web pages, console, updates over WiFi and from GitHub releases.

## Web pages

| Page | |
|---|---|
| `/` | Klospülung: settings of the flush program, manual control, emergency stop, live status |
| `/settings` | WiFi, device name, access point, admin password |
| `/update` | Firmware upload and updates from GitHub releases |
| `/console` | Web console (same commands as USB serial: `help`, `info`, `debug`) |
| `/memory`, `/info` | Stored values, device info and log |

The device always opens its access point at start and closes it after 5 minutes without
anybody connected.

## Flush program

Started by the push button or "Spülen" on the home page:

1. Valve opens until the pre-water amount has flowed (or its timeout).
2. Pump starts; valve stays open until the flush-water amount has flowed in total (or its
   timeout), then closes.
3. Pump keeps running for the pump delay, then stops.
4. Valve opens again until the end-water amount has flowed (or its timeout).

"Not-Stop" closes the valve, stops the pump and ends a running program. "Wasser" and
"Pumpe" run while held down and stop by themselves a second after the browser stops
responding.

## Updating from firmware 1.1 or older

Values stored by older firmware (water amounts, timeouts, WiFi) are kept. The filesystem image
(`spiffs.bin`) is no longer needed; all pages are part of the firmware.

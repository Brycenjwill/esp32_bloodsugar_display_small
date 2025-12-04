# ESP32 Nightscout Blood Sugar Display
## Features
- Pulls live SGV (blood glucose) values from Nightscout.
- Shows trend arrows (↗︎ ↑ → ↓ ↘︎ ⇈ ⇊).
- Updates automatically every minute and a half.
- Supports a current bg range indicator + an arrow.
- Works anywhere with Wi-Fi access.


## Requires
- A Nightscout server. Nightscout Pro was used for testing.
  - Target high and target low values should be set on your profile for the curreng bg range indicators to work.
- An ESP32. An ESP Devkit V4 was used for testing.
- 1 MAX7219 8×8 LED matrix.
- Either the ESP-IDF VSCode extension or the ESP-IDF Eclipse plugin for flashing

## Setup
### Wiring
| MAX7219 Pin | ESP32 Pin   | Description                  |
| ----------- | ----------- | ---------------------------- |
| **VCC**     | **5V**      | Power supply for LED matrix  |
| **GND**     | **GND**     | Ground connection            |
| **DIN**     | **GPIO 23** | SPI MOSI — Data into MAX7219 |
| **CS**      | **GPIO 15** | Chip Select (LOAD)           |
| **CLK**     | **GPIO 18** | SPI Clock                    |

### main/secrets.h
| Definition Name | Description                                                    | Example Value                             |
| --------------- | -------------------------------------------------------------- | ----------------------------------------- |
| `API_HOST`      | Base URL of your Nightscout server (no trailing slash)         | `"https://your-nightscout.herokuapp.com"` |
| `API_ENDPOINT`  | Nightscout REST endpoint for SGV data (example is what I used) | `"/api/v1/entries.json?count=1"`          |
| `API_ENDPOINT`  | Nightscout REST endpoint for Profile data (example is what I used) | `"/api/v1/profile.json?count=1"`          |
| `API_SECRET`    | Nightscout API secret                                          | `"yourapikeyhash"`                        |
| `WIFI_SSID`     | Wi-Fi network name                                             | `"HomeNetwork"`                           |
| `WIFI_PASSWORD` | Wi-Fi network password                                         | `"SuperSecret123"`                        |

## Future plans
- Add ability to set wifi intialization and api information without needing to flash.
- Add led based status indicators.

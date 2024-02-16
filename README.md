# ESPWeather
Dual Display Weather Station with Time and Image Showcase Capability.

## Features
### General:
- Automatic Time and Weather Updates
- WiFi Manager ([Library by tzapu](https://github.com/tzapu/WiFiManager))
- Some Battery Life Optimizations (**WIP**)
- SD Card Support
- Weather Forecast (**WIP**, the data exists just not the GUI)
  
### OLED Display:
- Current Time, Temperature and Weather Status
- Virtual Sun
- Dimming After 15 Seconds of Inactivity

### TFT Display:
- Picture Frame Functionality (from the SD Card, updates when time changes)
- Touch Screen Menu for Navigating the Forecast Graphs (**WIP**, placeholder)

## Requirements:
- PlatformIO
- ESP32 (I used the 38 pin variant)
- OLED Screen with *SH1106* Driver
- TFT Screen with *ILI9488* Driver (Can be changed at 'lib\TFT_eSPI-2.2.0\User_Setup').

## Connections
**Since I have developed this project on the 38 pin variant, values should be changed in case of missing pins either in 'src\main' or in 'lib\TFT_eSPI-2.2.0\User_Setup'.**

**!** You should edit lines 47 and 48 in 'src\main' with your own location for time and weather since it defaults to Bucharest, Romania. **!**

| Peripheral Pin | ESP32 Pin |
| :---: | :---: |
|OLED GND|GND (any)|
|OLED VCC|3V3|
|OLED SCL|22|
|OLED SDA|21|
|TFT VCC|3V3|
|TFT GND|GND (any)|
|TFT CS|15|
|TFT RST|-|
|TFT DC|2|
|TFT MOSI|23|
|TFT SCK|18|
|TFT LED|3V3|
|TFT MISO|-|
|TFT T_CLK|18|
|TFT T_CS|27|
|TFT T_DIN|23|
|TFT T_DO|19|
|TFT T_IRQ|-|
|SD_CS|5|
|SD_MOSI|23|
|SD_MISO|19|
|SD_SCK|18|

Any of the '3V3' pins can be replaced by **pin 32** or any other pin set HIGH in case the 3V3 pin can't output enough current.

## Libraries and credits
- [TFT_eSPI-2.2.0 by Bodmer](https://github.com/Bodmer/TFT_eSPI)
- [JSON for Modern C++ by nlohmann](https://github.com/nlohmann/json)
- [Adafruit_BusIO](https://github.com/adafruit/Adafruit_BusIO)
- [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit_SH1106 by wonho-maker](https://github.com/wonho-maker/Adafruit_SH1106)
- [ESP32Time by fbiego](https://github.com/fbiego/ESP32Time)
- [SdFat by greiman](https://github.com/greiman/SdFat)
- [TaskScheduler by arkhipenko](https://github.com/arkhipenko/TaskScheduler)
- [TJpg_Decoder by Bodmer](https://github.com/Bodmer/TJpg_Decoder)
- [Unity by ThrowTheSwitch](https://github.com/ThrowTheSwitch/Unity)
- [WiFiManager by tzapu](https://github.com/tzapu/WiFiManager)
- [OpenMeteo](https://open-meteo.com) for their weather API
- [WorldTimeAPI](http://worldtimeapi.org/) for their time API
  

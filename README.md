
# Fun with P4 Matrix 64x32 LED display

Project uses P4 Matrix 64x32 LED display with thermometer and internet access (for time, weather) controlled by ESP8266 platform. Created for my dad.

<!-- TODO: image or two -->

##### Features

+ Time synchronization using minimalistic NTP.
+ Digital clock.
+ Thermometer reading and display.
+ Portal website hosted over Wi-Fi with status and configuration.
+ Most of UI uses Polish language.





## Hardware

+ [P4 Matrix 64x32 LED display (`P4-1921-64x32-8S-S1`)](https://vi.aliexpress.com/item/1005005293054289.html)
+ [NodeMCU v3 board with ESP-12E (ESP8266) microcontroller](https://mischianti.org/2022/02/09/nodemcu-v3-high-resolution-pinout-and-specs/)
+ [Thermometer: DS18B20 (via OneWire on PIN D3)](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf)





## Software

[Platform IO](https://platformio.org/platformio-ide) is used with Arduino framework for development.


### Modules

* Web server (serving static content, status & assets; handling config & uploaded assets re-encoding);
* Network code (connect to configured network, incl. IP configuration; or host AP);
* NTP code;
* Display code

### 
<!-- TODO: ... -->

### Configuration

Configuration is divided between EEPROM and file-system based. EEPROM contains configuration related to basic operation, including networking, timezones, weather location and more. File system contains pages configurations and assets (including bitmaps for backgrounds).

#### EEPROM

EEPROM settings can be set by accessing HTTP `/config` endpoint (GET/POST) and are saved only if `save=true` is provided along the overridden settings. Detailed EEPROM in-memory layout is defined in [`config.hpp`](src/config.hpp) file. Some settings of `/config` are missing in the layout due being saved by internal platform code, i.e. network SSID & password.

```json
"_ TODO: add pretty-printed example /config"
```

#### Page configuration

Each page is configured via binary file at `/pages/0/config` where `0` is page ID/number. Each page can have analog clock and have up to 7 sprites that can be text,  special character, time (formatted as text), image or animation. Exact format is defined in [`pages.hpp`](src/pages.hpp) file. For example, time-based text sprite that uses `strftime` with custom extensions (`%o` or `%O` for Roman numeral month) can be used to create digital clock and/or display dates.

+ All file-system names are lower-case only.
+ Weather types for file names:
	+ `unknown` (default/secondary fallback)
	+ `sunny`
	+ `moony` (sunny at night)
	+ `a-bit-cloudy`
	+ `cloudy`
	+ `rainy` (high chance for rain)
	+ `rain` (heavy rain)
	+ `rainbow` (raining, but sunny)
	+ `snowy`
	+ `snow`
	+ `storm` (very windy with heavy rain)
	+ `thunder` (little rain)
	+ `thunderstorm`
	+ `blizzard` (snow storm)
	+ `windy`
+ Months file names use english full names.
+ Season names: `spring`, `summer`, `fall`, `winter`.

#### Bitmaps encoding

Bitmaps can be used as assets for backgrounds/animations/sprites. Stored bitmaps should be encoded as simple [BMP file](https://en.wikipedia.org/wiki/BMP_file_format) with standard `BITMAPINFOHEADER`, with 16 bits per pixel (confirm to RGB565 used by the display PxMatrix library; as opposed to default 24 bits), with mask specified, with no compression. The encoding is assured by re-encoding when handling uploads by web server and when [uploading file-system image by PlatformIO](https://docs.platformio.org/en/latest/platforms/espressif8266.html#using-filesystem) `pre` script.

Bitmaps can be "soft" symlinked, if the file contains path (content starting with `/` instead `BM` of regular BMP file header). Bitmaps can be used for animations, if so, often frame duration can be specified from inside file by reusing file header reserved fields (`uint16_t` right after file size).





## Notes

+ A lot of code (and directly many issues & possible improvements) shared with [my other project: AquariumController](https://github.com/AgainPsychoX/AquariumController). 
+ Display-related code based on PxMatrix examples, including [Aurora Demo](https://github.com/2dom/PxMatrix/blob/0c7c63c0248321a31dedcefcdaebc87df4624141/examples/Aurora_Demo/Aurora_Demo.ino). Specific library version from GitHub is used, PlatformIO registry has old version (with same internal version number XD)
+ [Interesting thing I found while working on project is you can use unsigned integer form of IPv4 addresses](https://www.browserling.com/tools/ip-to-dec).
+ Some WiFi names/passwords (incl. `$` or `-`?`) seems not work for some reason.
+ Problems using `GPIO9` and `GPIO10` for OneWire (constant crashes or upload errors)
	+ [Some people](https://www.letscontrolit.com/forum/viewtopic.php?t=1462) suggest using [DIO](https://hackaday.com/2017/10/01/trouble-flashing-your-esp8266-meet-dio-and-qio/) [flashing mode](https://docs.platformio.org/en/stable/platforms/espressif8266.html#flash-mode).



### To-do

+ Solve Sprites construction issues

+ Implement pages system as described in README above ;)
	+ Load initial page
	+ Function to change page
	+ Schedule next page changes
	+ Should all time be spent on updating the display? Or some smart subscribing mechanism for temperature/time updates?
	+ Upload example config & BMP file https://docs.platformio.org/en/latest/platforms/espressif8266.html#using-filesystem
	+ Debug & test simple image background
	+ Soft symlinking paths in pages config
	+ Read frame durations from `Animation` struct
+ Analog clock
	- Background buffer
	- Clearing as replacing stuff with buffer
	- Temporary code to setup temporary monochromatic buffer
	- Draw 3 lines from center as the time goes
	- Start the lines from center 2x2 pixels?
+ HTTP server
	+ Get current state as image
		- Access display buffer
		- Define request handler with right response type
		- Encode the buffer into BMP for output
	+ Filesystem? (instead embedding bytes into code)
+ Weather info
	+ Find service/API
	+ Show as text
	+ Show with icon
	+ Show with animated icon
	+ Configurable location
+ Clock background
	+ Configurable by image file (BMP)
	+ Download from external (periodic; from configured address)
	+ [WEBP for animations](https://discuss.tidbyt.com/t/gif-vs-webp/694/3) 
+ Themed mini-clocks (nice images/animations with small digital clock)
	+ Day (sun & clouds)
	+ Night (moon & stars)
	+ Snow
	+ Aquarium
+ Drawable canvas (via web)
	+ Upload file
	+ Color pick and drawing
	+ Live preview
	+ Fill by color
	+ Undo/redo
+ Rewrite the display library? Low code quality, draw pixel optimizations possible?
+ Use [better library for web server](https://github.com/me-no-dev/ESPAsyncWebServer).
+ Snake game? ;)
+ Curious...
	+ Does `std::setlocale` from `<clocale>` works?

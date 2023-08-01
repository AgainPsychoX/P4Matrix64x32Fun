
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

* Web server;
* Network code (connect to configured network, incl. IP configuration; or host AP);
* NTP code;

<!-- TODO: ... -->





## Notes

+ A lot of code (and directly many issues & possible improvements) shared with [my other project: AquariumController](https://github.com/AgainPsychoX/AquariumController). 
+ Display-related code based on PxMatrix examples, including [Aurora Demo](https://github.com/2dom/PxMatrix/blob/0c7c63c0248321a31dedcefcdaebc87df4624141/examples/Aurora_Demo/Aurora_Demo.ino). Specific library version from GitHub is used, PlatformIO registry has old version (with same internal version number XD)
+ [Interesting thing I found while working on project is you can use unsigned integer form of IPv4 addresses](https://www.browserling.com/tools/ip-to-dec).
+ Some WiFi names/passwords (incl. `$` or `-`?`) seems not work for some reason.
+ Problems using `GPIO9` and `GPIO10` for OneWire (constant crashes or upload errors)
	+ [Some people](https://www.letscontrolit.com/forum/viewtopic.php?t=1462) suggest using [DIO](https://hackaday.com/2017/10/01/trouble-flashing-your-esp8266-meet-dio-and-qio/) [flashing mode](https://docs.platformio.org/en/stable/platforms/espressif8266.html#flash-mode).



### To-do

+ Analog clock
	- Background buffer
	- Clearing as replacing stuff with buffer
	- Temporary code to setup temporary monochromatic buffer
	- Draw 3 lines from center as the time goes
	- Start the lines from center 2x2 pixels?
+ Use DS (GPIO 10) for temperature reading
	- Find the right library
	- Place it somewhere on the display
	- Value smoothing?
	- Test & commit
+ HTTP server
	+ Status (time, RSSI, ...)
		- Usual stuff, just copy code ;)
	+ Networking settings
		- For now could be left untouched, just hardcode stuff
		- Later: maybe copy & adapt code from YellowToyCar (and partially from AquariumController?)
	+ Get current state as image
		- Access display buffer
		- Define request handler with right response type
		- Encode the buffer into BMP for output
	+ Filesystem? (instead embedding bytes into code)
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
+ Weather info
+ Snake game? ;)



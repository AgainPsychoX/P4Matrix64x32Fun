
# P4 Matrix 64x32 LED display

<!-- TODO: short description -->

<!-- TODO: image or two -->





## Hardware

* P4 Matrix 64x32 LED display
* NodeMCU v0.9 (ESP8266)

<!-- TODO: links and more specific info -->





## Software

<!-- TODO: ... -->





## Notes

* Display-related code based on PxMatrix examples, including [Aurora Demo](https://github.com/2dom/PxMatrix/blob/0c7c63c0248321a31dedcefcdaebc87df4624141/examples/Aurora_Demo/Aurora_Demo.ino). Specific library version from GitHub is used, PlatformIO registry has old version (with same internal version number XD)
* [Interesting thing I found while working on project is you can use unsigned integer form of IPv4 addresses](https://www.browserling.com/tools/ip-to-dec).
* Test whenever **not** using Arduino `WiFi.*` makes it get optimized away. If so, drop it.
* Some WiFi names/passwords (incl. `$` or `-`?`) seems not work for some reason.



### To-do

* Working display
* Simplest WiFi connectivity
* NTP client -> time synchronization
* Digital clock
* Analog clock
* HTTP server
	* Status (time, RSSI, ...)
	* Networking settings
* Background
	* Upload BMPs
	* Download BMPs (periodic from configured address)
	* [WEBP for animations](https://discuss.tidbyt.com/t/gif-vs-webp/694/3) 
* Snake game controlled by UDP? ;)



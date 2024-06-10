
# P4 Matrix 64x32 LED display driver

[Original PxMatrix](https://github.com/2dom/PxMatrix) works for me, but feels slow and there seem to be too much flicker, so this mini-library is attempt to mitigate that.

Hardware:
+ [P4 Matrix 64x32 LED display (`P4-1921-64x32-8S-S1`)](https://vi.aliexpress.com/item/1005005293054289.html)
+ [NodeMCU v3 board with ESP-12E (ESP8266) microcontroller](https://mischianti.org/2022/02/09/nodemcu-v3-high-resolution-pinout-and-specs/)

Settings:
+ Row pattern: 8
+ Mux pattern: Binary
+ Scan pattern: Line
+ Block pattern: ABCD
+ Driver chip: Shift
+ Color order: RGB
+ Panels width: 1
+ Width: 64
+ Height: 32

## Notes

LEDs in the display have no color depth - they are either on or off. Color depth is simulated by limiting the time they are kept light.

### Wiring

Data to the display is provided by HUB75 interface. Display input HUB75 gets:
+ A/B/C/D/E signals for row pattern selection (binary addressing), 
+ latch and output enable signals, 
+ and finally clock and data: which is send serially:
	+ `MOSI` to `I R1`,
	+ `O R1` is then used to fed `I R2`, 
	+ `O R2` to `I G1`, 
	+ `O G1` to `I G2`, 
	+ `O G2` to `I B1`, 
	+ `O B1` to `I B2`, and it's done.
+ If using ESP8266, avoid using GPIO 16 for A/B/C/D/E; usable for latch & output enable.

HUB75:
```
+-----------+
| R1  . G1  |
| B1  . GND |
| R2  . G2  |
  B2  . GND |
  A   . B   |
| C   . D   |
| CLK . LAT |
| OE  . GND |
+-----+-----+
```

### Other

+ There is [nice RGB565 color picker](https://rgbcolorpicker.com/565) online.

### To-do

+ Try `((r >> i) & 1)` instead `(r & (1 << i))`
+ Double buffer
	+ Mode 1. Buffering encoded ready to display to the screen.
	+ Mode 2. Buffering as original (RGB565 / `uint16_t`) then bulk encoding.
	+ Mode 3. Like mode 2., but use dirty flags to allow partial encoding.
+ Possible more optimizations? See https://github.com/2dom/PxMatrix/pull/24/commits/bf9898040d1f3f7aecc212b3178603b773aacfd6
	+ Somehow, `fillRect` is only slightly slower to `drawRect` (not filled), despite huge number of pixels drawn difference. Investigate why.
	+ Some other specialized functions?
+ For debug in `displayStep`: count cycles instead microseconds (can use `ESP.getCycleCount()` under ESP8266) & try balance the `if`s
+ Allow color depth to be run-time configurable (doesn't seem to be critical for performance
+ Test (and adapt) the display library to lower C++ standard.
+ Rotation support

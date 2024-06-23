
## Hardware

### Microcontroller pins usage

| Pin | GPIO | Notes                                                          |
|-----|------|----------------------------------------------------------------|
|  A0 |      | Analog to Digital Converter; input only; Unused for now        |
|   G |      | Ground, but some docs suggest it's related to ADC as well...   |
|  VU |      | Voltage from USB, most likely around 5V                        |
|  S3 |   10 | PWM-able;                                                      |
|  S2 |    9 | PWM-able;                                                      |
|  S1 |    8 | MOSI                                                           |
|  SC |   11 | CS0                                                            |
|  S0 |    7 | MISO                                                           |
|  SK |    6 | SCLK                                                           |
|   G |      | Ground                                                         |
| ... |      |                                                                |
|  D0 |   16 | Accessed by special register; Wake signal; Used for display latch. |
|  D1 |    5 | PWM-able; Used for display row selection (Pin A).               |
|  D2 |    4 | PWM-able; Used for display row selection (Pin B).               |
|  D3 |    0 | PWM-able; High for run (default), low for flash mode; Used for OneWire for DS18B20 thermometer. |
|  D4 |    2 | PWM-able; Built-in LED, driven by low (pulled-up by default); Used for display output enable. |
|  3V |      | 3V3, from voltage stabilizer                                   |
|   G |      | Ground                                                         |
|  D5 |   14 | PWM-able; HSPI SCK; Used to push pixels data to the display.   |
|  D6 |   12 | PWM-able; HSPI MISO; Used for display row selection (Pin D); Maybe unnecessary actually. |
|  D7 |   13 | PWM-able; HSPI MOSI; Used to push pixels data to the display.  |
|  D8 |   15 | PWM-able; HSPI CS; Pulled-down by default, must be low on start; Used for display row selection (Pin C). |
|  RX |    3 | PWM-able; Used for UART0 connection, default via USB           |
|  TX |    1 | PWM-able; Used for UART0 connection, default via USB           |
|   G |      | Ground                                                         |
|  3V |      | 3V3, from voltage stabilizer                                   |
|     |      |                                                                |





## Notes

+ There is [nice RGB565 color picker](https://rgbcolorpicker.com/565) online.
+ How `yield`ing works in ESP8266?
	+ ESP8266 NonOS SDK, which the Arduino Core is built-on, has some kernel system (SYS stack), which runs user code (user/CONT stack), and expects the control to be back, at very least once per 500 milliseconds.
	+ See `loop_task` and `loop_wrapper()` to see how the user code wrapper looks like. The Arduino `setup()` and `loop()` are called there.
	+ `yield()` returns the control back Actually it's more complicated: 
		+ `esp_schedule()` schedules execution of the loop code - which is required if you want execution to return to user code.
		+ `esp_yield()` <sub>(actually `esp_yield_within_cont()` but it's inlined)</sub> yields execution to saved point in kernel code. In turn, it's actually:
			+ `cont_can_yield()` - check if yield is possible; mostly for sanity I guess <sub>(because you shouldn't yield in ISR, and the `cont_t` struct should be initialized while in the user code anyway)</sub>.
			+ `cont_yield(g_pcont);` - which actually yields the execution;
			+ `s_cycles_at_yield_start = ESP.getCycleCount();` - prepares variable used later on in `optimistic_yield`.
			+ `run_scheduled_recurrent_functions();` (see below).
	+ `optimistic_yield()` is like `yield()` but only conditionally, if specified time passed from last yield. It exist to try avoid context switching penalty I think.
	+ `run_scheduled_functions();` - called on loop end; full explanation comments in `Schedule.h` in sources of Arduino core for ESP8266; feels a bit useless.
	+ `run_scheduled_recurrent_functions()` - like above, but also on `yield`; from the comments: used to "independently execute user code in CONT stack on a regular basis".
	+ `Ticker`/`os_timer_setfn`+`os_timer_arm` is handled by SYS stack, so requires the `esp_yield()`;
	+ `schedule_recurrent_function_us` with manual `run_scheduled_recurrent_functions` instead full `yield`s feels smoother in some applications. Note: it still does normal `yield`s every 100 milliseconds.
	+ Both methods (from my experience: sometimes, if it lags for some reason), ticks can occur right after each other to "cover up" for the missed timing.
	+ Further read:
		+ Source code of Arduino core for ESP8266: `core_esp8266_main.cpp`, `ets_sys.h`, `cont.h`, `cont.S`, `cont_util.cpp`, `Schedule.cpp`, `Schedule.h`, ...
		+ Articles at https://sub.nanona.fi/esp8266/ 
		+ ESP8266 ROM (kernel code parts) dump: https://df.lth.se/~kongo/esp8266.bin/iram0.txt 
+ Decompiling ESP8266 code
	+ Bugged? See https://stackoverflow.com/questions/72064789/curly-brackets-in-xtensa-dissasembly and https://www.esp8266.com/viewtopic.php?f=9&t=3105&p=18572&hilit=section+that+denotes#p18572 and https://sourceware.org/pipermail/binutils-cvs/2018-June/048351.html 



### To-do

1. Try out `yield`ing even more, resolve late tick situations to reduce blinking even more.
	+ Instead of waiting, maybe do display tick with scaled down show time, keeping the same ratio?
	+ ...?
2. Try `((r >> i) & 1)` instead `(r & (1 << i))`
3. Add networking code, web server and NTP, digital clock
4. Add file system, include analog clock BMP and use it
5. Colors scaling using precalculated table?

+ Write up proper README.
+ Migrate old project version code here, with new library.
+ Rewrite some parts of OneWire library to avoid wasting time, like `delayMicroseconds` in single `read_bit`.
+ PlatformIO `monitor_filter = send_on_enter` is so useful, but annoying, because of lack of backspace (and delete) support.
+ Figure out why `_BSD_SOURCE` is set as defined. Forgot to document that anywhere...

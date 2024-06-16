
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
	+ Further read:
		+ Source code of Arduino core for ESP8266: `core_esp8266_main.cpp`, `ets_sys.h`, `cont.h`, `cont.S`, `cont_util.cpp`, `Schedule.cpp`, `Schedule.h`, ...
		+ Articles at https://sub.nanona.fi/esp8266/ 
		+ ESP8266 ROM (kernel code parts) dump: https://df.lth.se/~kongo/esp8266.bin/iram0.txt 

### To-do

1. Try out `yield`ing even more, resolve late tick situations to reduce blinking even more.
	+ Maybe the 100us diff (before `yield()` -> (late) tick) is because of context switching?
	+ Try figure out how `yield` works at all in the ESP8266, see sources/assembly
	+ Try using `esp_yield_within_cont` instead `yield`?
2. Try using `schedule_recurrent_function_us` instead the `Ticker`?
3. Maybe separate out the tick, and call it manually too?
4. Try `((r >> i) & 1)` instead `(r & (1 << i))`
5. Add networking code, web server and NTP, digital clock
6. Add file system, include analog clock BMP and use it

+ Write up proper README.
+ Migrate old project version code here, with new library.
+ Try `yield` more, and/or try avoid critical/no interrupts sections.
+ PlatformIO `monitor_filter = send_on_enter` is so useful, but annoying, because of lack of backspace (and delete) support.
+ Figure out why `_BSD_SOURCE` is set as defined. Forgot to document that anywhere...

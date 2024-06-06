
## Notes

+ There is [nice RGB565 color picker](https://rgbcolorpicker.com/565) online.

### To-do

+ For debug in `displayStep`: count cycles instead microseconds & try balance the `if`s
+ Write up proper README.
+ Migrate old project version code here, with new library.
+ Try `yield` more, and/or try avoid critical/no interrupts sections.
+ Allow color depth to be run-time configurable (doesn't seem to be critical for performance)
+ Test (and adapt) the display library to lower C++ standard.
+ PlatformIO `monitor_filter = send_on_enter` is so useful, but annoying, because of lack of backspace (and delete) support.


### To-do

1. Fix colors
	+ Add extra `asm volatile ("nop");` to balance out `if` paths
	+ Maybe dump buffer in old and new and compare
	+ Maybe show time is scaled with invalid proportions?
	+ Maybe it's show time issue out of sync with rows?

+ Write up proper README.
+ Migrate old project version code here, with new library.
+ Try `yield` more, and/or try avoid critical/no interrupts sections.
+ Test (and adapt) the display library to lower C++ standard.
+ PlatformIO `monitor_filter = send_on_enter` is so useful, but annoying, because of lack of backspace (and delete) support.

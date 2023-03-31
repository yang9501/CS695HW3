Youtube link for demo:


This implementation uses a binary flag to indicate the watch's running state.  This and the counter that serves as the timer are controlled by mutexes to ensure consistent states when operations are being performed.  

Four threads are created:  two threads for each of the start/stop and reset buttons, one for the timer updater, and the last for the display output.  The two button threads have the highest priority so that any action can be reflected as fast and accurately as possible to the moment it is inputted.  Next highest is the timer updater, so that the time can be tracked accurately.  Finally, the last in priority is the display output thread, because this functionality doesn't strictly depend on time for precision.



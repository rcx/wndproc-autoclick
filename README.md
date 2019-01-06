# WndProc Autoclicker

An autoclicker is a program that simulates mouse input to click automatically for you.
Typically this is used to automate some boring task (like Cookie Clicker games).

The typical way to do that on Windows is to use the Winapi functions SendMessage or SendInput`.
However, what if we want to go even faster? Like, really, really fast.

This project is a rough PoC for bypassing the Win32 message pump entirely by calling the target program's WndProc function directly to post click events, using code injection.
The upside is that it's roughly twice as fast as using PostMessage or SendMessage. The downside is that it's much less safe.
I didn't spend much time debugging this and the code is terrible, but it technically is possible and works.

## Methods
First, we grab the HWND underneath the cursor and find the process it belongs to.
Then we inject a DLL into this process that will do our autoclicking using a standard LoadLibrary injection.
In the DLL, we first grab the HWND's WndProc's address. Then, there are two options.
For simple programs (like Windows Calculator or some simple .NET Forms apps), we can simply call WndProc as quickly as possible from our DLL's thread.
For complex programs (like Chromium or Firefox), trying to call WndProc from the wrong thread results in a crash. We need to call it from the program's GUI thread.
To do that, we get the thread ID associated with the HWND then hijack it using SetThreadContext. This method is a lot less stable.

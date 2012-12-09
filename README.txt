Compiled with MSVC++ 6.0

I will convert this to a OllyDbg v2.0 plugin when I find a stealth plugin for 
it that works for me.

Use Shift+2 to set a continue point (or from the right-click menu). To view 
continue points, you may open it from the plugins menu.

I made this because when searching by an on-access bp, it triggered something 
in the main loop. To continue flow and allow me to find the code that I 
actually wanted, something that could automatically continue would be nice. 
Then I made this.

TODO: For some reason the window does not save the fact that it was open when 
you close OllyDbg, so it does not restore it.

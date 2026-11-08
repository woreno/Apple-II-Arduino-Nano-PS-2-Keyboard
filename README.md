# Apple II Arduino Nano PS/2 Keyboard description
A simple mod to connect a PS/2 Keyboard along the factory Apple II keyboard. When theres activity in the PS/2 keyboard the 244 (or 245) buffer is blocked so it lets arduino keystrokes to pass to the Apple II board; after some period of inactivity in the PS/2 the control is transfered to Apple II keyboard. 
This was my first mod for my own built Apple II computer, so the keyboard socket/connector are IDC, however in the original Apple II computer these are wider (DIP socket/connector).

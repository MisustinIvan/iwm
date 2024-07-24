# IWM - Ivan's Window Manager
IWM is a simple window manager for X11 written in C with [Xlib](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html).It has roughly 550 lines of code(with coments and empty lines). It is inspired by dwm. It is designed to be a bare-bones start for people to build upon as they need. It operates on a single linked list of fullscreen windows, which the user can move around and swap. It has a simple statusbar at the top of the screen that displays the list of windows with the focused one being highlighted.

# Installation
To install IWM, clone the repository and run `make build` in the root directory. This will create the `iwm` executable. To install the executable, run `sudo make install`.

# Configuration
IWM is configured by directly editing the source code. There are some configuration options at the top of `main.c` that can be changed to suit your needs. The configuration options are:
- `border_width`: The width of the window borders in pixels.
- `bar_height`: The height of the statusbar.
- `fontname`: The font used to draw the statusbar.
- `fg_color_const`: Foreground color.
- `bg_color_const`: Background color.
- `primary_color_const`: Primary accent color.

Some other options have to be configured in other places, which I have not yet refactored into variables. These include the terminal emulator and run menu.

## Keybindings
Keybindings are configured in the `grabkeys` and `keypress` functions in `main.c`. The default keybindings are:
- `MODKEY + Enter`: Open terminal emulator.
- `MODKEY + q`: Close focused window.
- `MODKEY + Space`: Open run menu.
- `MODKEY + k`: Focus window to the left.
- `MODKEY + l`: Focus window to the right.
- `MODKEY + Shift + k`: Swap with left window.
- `MODKEY + Shift + l`: Swap with right window.
- `MODKEY + Control + r`: Restart the wm.
- `MODKEY + Control + Shift + q`: Quit the wm.

# Screenshots
![Screenshot 1](./screenshots/screenshot1.png)

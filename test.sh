#!/bin/bash

Xephyr +xinerama -screen 800x600 -screen 800x600 -ac :100 &

sleep 0.1
DISPLAY=:100 ./iwm &
sleep 0.2
DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &

#!/bin/bash

Xephyr -ac -br -noreset -screen 1600x1000 -host-cursor :100 &
sleep 0.1
DISPLAY=:100 ./iwm &
sleep 0.2
DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &
#DISPLAY=:100 alacritty &

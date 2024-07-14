#!/bin/bash

Xephyr -ac -br -noreset -screen 800x600 -host-cursor :100 &
sleep 0.1
DISPLAY=:100 ./iwm &
sleep 0.1
DISPLAY=:100 dolphin &
DISPLAY=:100 dolphin &
DISPLAY=:100 dolphin &
DISPLAY=:100 dolphin &
DISPLAY=:100 dolphin &

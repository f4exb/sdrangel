#!/bin/sh

COMMAND="/bin/bash"

xhost +si:localuser:root # allow connections to X server
xhost +si:localuser:${USER}
docker run --privileged -e "DISPLAY=unix:0.0" -p 50022:22 -v="/tmp/.X11-unix:/tmp/.X11-unix:rw" -v="/home/${USER}/.config/f4exb:/home/sdr/.config/f4exb:rw" -i -t --rm sdrangel/bionic:vanilla $COMMAND

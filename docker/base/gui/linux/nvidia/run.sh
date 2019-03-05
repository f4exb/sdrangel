#!/bin/sh

COMMAND="/bin/bash"
USER_UID=$(id -u)

xhost +si:localuser:root # allow connections to X server
xhost +si:localuser:${USER}
#docker run --privileged -e "DISPLAY=unix:0.0" -p 50022:22 -v="/tmp/.X11-unix:/tmp/.X11-unix:rw" -v="/tmp/debug:/opt/build/sdrangel/debug:rw" -i -t --rm sdrangel/bionic:linux_nvidia $COMMAND
docker run -it --rm --privileged \
    -e "PULSE_SERVER=unix:/run/user/1000/pulse/native" \
    -e "DISPLAY=unix:0.0" \
    -p 50022:22 \
    -v="/tmp/.X11-unix:/tmp/.X11-unix:rw" \
    -v="/home/${USER}/.config:/home/sdr/.config:rw" \
    -v="/run/user/${USER_UID}/pulse:/run/user/1000/pulse" \
    sdrangel/bionic:linux_nvidia $COMMAND

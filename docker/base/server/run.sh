#!/bin/sh

COMMAND="/bin/bash"

docker run --privileged -p 50022:22 -v="/home/${USER}/.config/f4exb:/home/sdr/.config/f4exb:rw" -i -t --rm sdrangel/bionic:server $COMMAND

#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p airspy && cd airspy

git clone https://github.com/airspy/airspyhf.git

cd airspyhf
mkdir -p build && cd build
cmake .. -DINSTALL_UDEV_RULES=ON
sudo make install
sudo ldconfig

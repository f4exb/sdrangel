#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p airspyhf && cd airspyhf

git clone https://github.com/airspy/airspyhf.git

cd airspyhf
git reset --hard "1.1.5"
mkdir -p build && cd build
cmake .. -DINSTALL_UDEV_RULES=ON
sudo make install
sudo ldconfig

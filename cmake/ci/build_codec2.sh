#!/bin/sh

sudo apt-get -y install libsamplerate0-dev libspeex-dev libspeexdsp-dev

cd $HOME
mkdir -p external && cd external
mkdir -p drowe67 && cd drowe67

git clone https://github.com/drowe67/codec2-dev.git codec2

cd codec2
git reset --hard "v1.0.3"
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p f4exb && cd f4exb

git clone https://github.com/f4exb/cm256cc.git

cd cm256cc
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

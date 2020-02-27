#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p f4exb && cd f4exb

git clone https://github.com/f4exb/serialDV.git

cd serialDV
git reset --hard "v1.1.4"
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

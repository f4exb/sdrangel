#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p szechyjs && cd szechyjs

git clone https://github.com/szechyjs/mbelib.git

cd mbelib
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

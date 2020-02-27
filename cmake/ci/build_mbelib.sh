#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p szechyjs && cd szechyjs

git clone https://github.com/szechyjs/mbelib.git

cd mbelib
git reset --hard 9a04ed5c78176a9965f3d43f7aa1b1f5330e771f
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

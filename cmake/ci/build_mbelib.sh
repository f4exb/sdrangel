#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p szechyjs && cd szechyjs

git clone https://github.com/szechyjs/mbelib.git

cd mbelib
git reset --hard fe83b32c6a60cdd7bce8cecf3c7a0b9ec87a7667
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

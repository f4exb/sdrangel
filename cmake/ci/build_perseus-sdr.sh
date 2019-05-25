#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p microtelecom && cd microtelecom

git clone https://github.com/Microtelecom/libperseus-sdr.git

cd libperseus-sdr
./bootstrap.sh
./configure
make
sudo make install
sudo ldconfig

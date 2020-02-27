#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p microtelecom && cd microtelecom

git clone https://github.com/f4exb/libperseus-sdr.git

cd libperseus-sdr
git checkout fixes
git reset --hard afefa23e3140ac79d845acb68cf0beeb86d09028
./bootstrap.sh
./configure
make
sudo make install
sudo ldconfig

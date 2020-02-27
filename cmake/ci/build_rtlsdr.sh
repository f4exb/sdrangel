#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p rtlsdr && cd rtlsdr

git clone https://github.com/osmocom/rtl-sdr.git

cd rtl-sdr
git reset --hard be1d1206bfb6e6c41f7d91b20b77e20f929fa6a7
mkdir -p build && cd build
cmake .. -DDETACH_KERNEL_DRIVER=ON -DINSTALL_UDEV_RULES=ON
sudo make install
sudo ldconfig

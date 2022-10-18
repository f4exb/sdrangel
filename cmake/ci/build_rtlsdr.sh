#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p rtlsdr && cd rtlsdr

git clone https://github.com/osmocom/rtl-sdr.git

cd rtl-sdr
git reset --hard 5e73f90f1d85d8db2e583f3dbf1cff052d71d59b
mkdir -p build && cd build
cmake .. -DDETACH_KERNEL_DRIVER=ON -DINSTALL_UDEV_RULES=ON
sudo make install
sudo ldconfig

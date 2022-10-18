#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p airspyhf && cd airspyhf

git clone https://github.com/airspy/airspyhf.git

cd airspyhf
git reset --hard 1af81c0ca18944b8c9897c3c98dc0a991815b686
mkdir -p build && cd build
cmake .. -DINSTALL_UDEV_RULES=ON
sudo make install
sudo ldconfig

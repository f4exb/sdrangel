#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p xtrx-sdr && cd xtrx-sdr

git clone https://github.com/xtrx-sdr/images.git xtrx-images

cd xtrx-images
git reset --hard 053ec82
git submodule init
git submodule update
cd sources
mkdir -p build && cd build
cmake -DENABLE_SOAPY=NO ..
sudo make install
sudo ldconfig
# cd images/binaries/Ubuntu_16.04_amd64
# sudo dpkg -i *.deb
# sudo apt-get -y install -f
# sudo ldconfig

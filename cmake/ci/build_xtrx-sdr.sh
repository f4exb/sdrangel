#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p xtrx-sdr && cd xtrx-sdr

git clone https://github.com/xtrx-sdr/images.git

cd images/binaries/Ubuntu_16.04_amd64
sudo dpkg -i *.deb
sudo apt-get -y install -f
sudo ldconfig

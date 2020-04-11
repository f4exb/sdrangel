#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p f4exb && cd f4exb

git clone https://github.com/f4exb/cm256cc.git

cd cm256cc
git reset --hard c0e92b92aca3d1d36c990b642b937c64d363c559
mkdir -p build && cd build
cmake ..
sudo make install
sudo ldconfig

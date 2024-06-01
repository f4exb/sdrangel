#!/bin/sh

cd $HOME
mkdir -p external && cd external
mkdir -p f4exb && cd f4exb

git clone https://github.com/ggerganov/ggmorse.git

cd ggmorse
mkdir -p build && cd build
cmake -DGGMORSE_BUILD_TESTS=OFF -DGGMORSE_BUILD_EXAMPLES=OFF ..
sudo make install
sudo ldconfig

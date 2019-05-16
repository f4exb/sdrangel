
cd $HOME/build
mkdir f4exb && cd f4exb

git clone https://github.com/f4exb/dsdcc.git

cd dsdcc
mkdir build && cd build
cmake ..
sudo make install

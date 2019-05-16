
cd $HOME/build
mkdir f4exb && cd f4exb

git clone https://github.com/f4exb/cm256cc.git

cd cm256cc
mkdir build && cd build
cmake ..
sudo make install

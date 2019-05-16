
sudo apt-get install libsamplerate0-dev libspeex-dev libspeexdsp-dev

cd $HOME/build
mkdir drowe67 && cd drowe67

git clone https://github.com/drowe67/codec2.git

cd codec2
mkdir build && cd build
cmake ..
sudo make install

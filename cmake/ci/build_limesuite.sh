cd $HOME
mkdir -p external && cd external
mkdir -p limesuite && cd limesuite

wget https://github.com/myriadrf/LimeSuite/archive/v20.01.0.tar.gz
tar -xf v20.01.0.tar.gz
cd LimeSuite-20.01.0
mkdir -p builddir && cd builddir
cmake ..
sudo make install
sudo ldconfig

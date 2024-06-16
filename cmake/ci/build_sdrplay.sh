cd $HOME
mkdir -p external && cd external
mkdir -p sdrplayapi && cd sdrplayapi

git clone https://github.com/srcejon/sdrplayapi.git
cd sdrplayapi
sed -i s/more\ -d/cat/ install_lib.sh
sudo yes | bash install_lib.sh

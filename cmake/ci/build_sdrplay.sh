cd $HOME
mkdir -p external && cd external
mkdir -p sdrplayapi && cd sdrplayapi

git clone https://github.com/srcejon/sdrplayapi.git
cd sdrplayapi
sudo yes | sh install_lib.sh

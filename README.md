# METARmap
METAR map created while learning to use rPi in C
To install and build:
mkdir dev
cd dev
git clone https://github.com/hfahle/rpi_ws281x
git clone https://github.com/hfahle/METARmap
sudo apt-get install libcurl4-openssl-dev
sudo apt-get cmake
cd rpi_ws281x
cmake .
make
cd ..
cd METARmap
make
sudo ./test
make release
sudo crontab -e

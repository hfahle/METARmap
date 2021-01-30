# METARmap
METAR map created while learning to use rPi in C  
To install and build:  
mkdir dev  
cd dev  
git clone https://github.com/hfahle/rpi_ws281x
git clone https://github.com/hfahle/METARmap  #  or if you are hfahle, git clone the ssh form here, after installing the ssh key on the new machine  
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
chmod +755 refresh.sh  
chmod +755 lightsoff.sh  
sudo crontab -e  

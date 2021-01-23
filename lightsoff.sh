/usr/bin/sudo pkill -F /home/pi/offpid.pid
/usr/bin/sudo pkill -F /home/pi/metarpid.pid
cd /home/pi/dev/METARmap && /usr/bin/sudo /home/pi/dev/METARmap/METARmap -n -c & echo $! > /home/pi/offpid.pid

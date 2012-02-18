#/usr/bin/expect

set timeout 100
spawn picocom -b 38400 /dev/ttyUSB0
expect "+READY"
expect "+OK"

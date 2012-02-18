#/usr/bin/expect

set timeout 100
spawn picocom -b 38400 /dev/ttyUSB0
expect -re IP:(.*)\r
set ip $expect_out(1,string)
expect "+READY"
spawn curl -v http://$ip/index.shtml
expect "</html>"
spawn curl -v http://$ip/buttons.shtml
expect "</html>"
spawn curl -v http://$ip/ok.txt
expect "+OK"

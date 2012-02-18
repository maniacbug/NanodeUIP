# Wireless Sensor Network Gateway

This example sketch shows how to use a Nanode (or Arduino connect to an
ENC28J60-based Ethernet Shield) as a gateway between your wireless sensor
network and Pachube.  

## Requirements

This sketch requires a special uIP configuration.  To upload this sketch
using the Arduino IDE, first copy the uip-conf.h file from this directory
up to the NanodeUIP directory.  (It's a good idea to keep a backup of the
old one!)

It also requires libraries for working with an RF Wireless Sensor Network.
* (RF24)[www.github.com/maniacbug/RF24]
* (RF24Network)[www.github.com/maniacbug/RF24Network]

To test out this example, put the 'sensornet' example from RF24Network onto
at least one node, set the address to 1-5, and start it up.

Then, attach an nRF24L01+ radio to your Nanode and upload this sketch.  Soon,
the readings from the other node(s) will be getting posted to Pachube every
four seconds.

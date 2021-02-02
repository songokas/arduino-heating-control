# Floor heating with arduino

## Project structure

* master - main heating controller
* slave - additional controller if master is not enough
* sender - sends temperature to master

## Software dependencies

* [Arduino.mk](https://github.com/sudar/Arduino-Makefile) for building/uploading
* download/install arduino editor


```
sudo apt-get install arduino-mk
cd install && wget https://downloads.arduino.cc/arduino-1.8.13-linux64.tar.xz && tar xf arduino-1.8.13-linux64.tar.xz
```


## Harware dependencies

* arduinos - for thinking/temperature transmission
* NRF24l01 - for communication
* DS18B20 - for reading temperature
* arduino with internet access for master - for ui control
* relays/mosfets - for acctuator control
* power sources for sender/slave/master
* power source for acctuators (24V or other)

## How it works

* each node (sender) repors temperatures to master
* master makes a decission based on json configuration/html form
* turns the main heater, acctuattors on/of
* sends data to slave to turn acctuators on/off if needed

###  Configuration

1 master 1 slave and many senders

* slave ids : 100 - 255
* master ids: 0 - 99

each sender defines an id which is a pin on the receiver (master, slave)

slave ids are mapped with adding 100

e.g. if slave controls with pin 5 sender id should be 105

master pins and sender ids match

configuration is controlled through http interface

## Howto

* install https://github.com/sudar/Arduino-Makefile
* git clone https://github.com/songokas/arduino-heating-control
* modify/change source code according to your needs e.g. (src/Config.h)
* build/upload sender
* build/upload master
* build/upload slave

### building sender

```
cd sender
cp Makefile-debian Makefile
# update paths/variables in Makefile save
# each node must have a different node id that should map on the receiving end
ENC_KEY="enckey" NODE_ID=5 make && make upload && make monitor
```

### building master

requires:

* arduino with internet access


```
cd master
cp Makefile-debian Makefile # or create/modify
# update paths/variables in Makefile save
# master NODE_ID is defined in Config.h
ENC_KEY="longlonglongpass" make && make upload && make monitor
```

find the ip of the master (nmap, router interface)

go to http://{master ip} with your browser

### building slave


```
cd slave
cp Makefile-debian Makefile
# update paths/variables in Makefile save
# slave NODE_ID is define in Config.h
ENC_KEY="longlonglongpass" make && make upload && make monitor
```

## Tested on

* master - Arduino Mega with internet shield
* slave - Arduino Nano
* sender - Arduino Nano, Pro Mini


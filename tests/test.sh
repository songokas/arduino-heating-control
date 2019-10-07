#!/bin/bash

set -e

cd build

mockPath="Mocks/";
options="-g -std=gnu++11"

includeOptions="-I $mockPath"

g++ $includeOptions $mockPath/Arduino.cpp -c $options
g++ $includeOptions $mockPath/fake_serial.cpp -c $options
g++ $includeOptions $mockPath/WString.cpp -c $options
g++ -DARDUINO=200 -I $mockPath ../libraries/Time/Time.cpp -c $options
g++ -I $mockPath -I ../libraries/xxtea-iot-crypt/src/ ../libraries/xxtea-iot-crypt/src/xxtea-iot-crypt.cpp -c $options
g++ -I $mockPath -I ../libraries/xxtea-iot-crypt/src/core/ ../libraries/xxtea-iot-crypt/src/core/xxtea_core.cpp -c $options

if [[ $2 ]]; then
    g++ -I ../libraries/Catch2/single_include/ main.cpp -c $options
fi

# compile classes first
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ ../src/Common.cpp -c $options
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ ../src/Config.cpp -c $options
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ ../src/Domain/HeaterInfo.cpp -c $options
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ ../src/Domain/ZoneInfo.cpp -c $options
g++ -I $mockPath -I ../libraries/xxtea-iot-crypt/src/ -I ../src/Network/ ../src/Network/Encryptor.cpp -c $options
g++ -I $mockPath -I ../src/ -I ../src/Network/ ../src/Network/PacketHandler.cpp -c $options

# compile test
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ -I ../libraries/Catch2/single_include/ Domain/HeaterInfoTest.cpp  -c $options
g++ -I $mockPath -I ../libraries/Time/ -I ../src/Domain/ -I ../src/ -I ../libraries/Catch2/single_include/ Domain/ZoneInfoTest.cpp  -c $options
g++ -I $mockPath -I ../src/Network/ -I ../libraries/Catch2/single_include/ Network/EncryptorTest.cpp  -c $options
g++ -I $mockPath -I ../src/ -I ../src/Network/ -I ../libraries/Catch2/single_include/ Network/PacketHandlerTest.cpp  -c $options
g++ Arduino.o fake_serial.o WString.o xxtea-iot-crypt.o xxtea_core.o Time.o Common.o Config.o HeaterInfo.o ZoneInfo.o Encryptor.o HeaterInfoTest.o ZoneInfoTest.o EncryptorTest.o PacketHandler.o PacketHandlerTest.o main.o -o test
./test $1

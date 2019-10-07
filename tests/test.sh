#!/bin/bash

set -e

if [[ ! -d build ]]; then
    mkdir build
fi

cd build

mockPath="../Mocks/";
options="-g -std=gnu++11"
librariesPath="../../libraries"
srcPath="../../src"
testPath=".."

includeOptions="-I $mockPath"

g++ $includeOptions $mockPath/Arduino.cpp -c $options
g++ $includeOptions $mockPath/fake_serial.cpp -c $options
g++ $includeOptions $mockPath/WString.cpp -c $options
g++ -DARDUINO=200 -I $mockPath $librariesPath/Time/Time.cpp -c $options
g++ -I $mockPath -I $librariesPath/xxtea-iot-crypt/src/ $librariesPath/xxtea-iot-crypt/src/xxtea-iot-crypt.cpp -c $options
g++ -I $mockPath -I $librariesPath/xxtea-iot-crypt/src/core/ $librariesPath/xxtea-iot-crypt/src/core/xxtea_core.cpp -c $options


if [[ ! -f main.o ]]; then
    g++ -I $librariesPath/Catch2/single_include/ $testPath/main.cpp -c $options
fi

# compile classes first
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath  $srcPath/Common.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath  $srcPath/Config.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath  $srcPath/Domain/HeaterInfo.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath  $srcPath/Domain/ZoneInfo.cpp -c $options
g++ -I $mockPath -I $librariesPath/xxtea-iot-crypt/src/ -I $srcPath/Network/ $srcPath/Network/Encryptor.cpp -c $options
g++ -I $mockPath -I $srcPath -I $srcPath/Network/ $srcPath/Network/PacketHandler.cpp -c $options

# compile test
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath -I $librariesPath/Catch2/single_include/ $testPath/Domain/HeaterInfoTest.cpp  -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Domain/ -I $srcPath -I $librariesPath/Catch2/single_include/ $testPath/Domain/ZoneInfoTest.cpp  -c $options
g++ -I $mockPath -I $srcPath/Network/ -I $librariesPath/Catch2/single_include/ $testPath/Network/EncryptorTest.cpp  -c $options
g++ -I $mockPath -I $srcPath -I $srcPath/Network/ -I $librariesPath/Catch2/single_include/ $testPath/Network/PacketHandlerTest.cpp  -c $options
g++ Arduino.o fake_serial.o WString.o xxtea-iot-crypt.o xxtea_core.o Time.o Common.o Config.o HeaterInfo.o ZoneInfo.o Encryptor.o HeaterInfoTest.o ZoneInfoTest.o EncryptorTest.o PacketHandler.o PacketHandlerTest.o main.o -o test
./test $1

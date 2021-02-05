#!/bin/bash

set -e

if [[ ! -d build ]]; then
    mkdir build
fi

cd build

mockPath="../Mocks/";
options="-g -std=gnu++11"
librariesPath="../../libraries"
linkPath="../../arduino-link"
srcPath="../../src"
testPath=".."

includeOptions="-I $mockPath"
testInclude="-I $librariesPath/Catch2/single_include/catch2"

g++ $includeOptions $mockPath/Arduino.cpp -c $options
g++ $includeOptions $mockPath/fake_serial.cpp -c $options
g++ $includeOptions $mockPath/WString.cpp -c $options
g++ -DARDUINO=200 -I $mockPath $librariesPath/Time/Time.cpp -c $options

if [[ ! -f main.o ]]; then
    g++ $testInclude $testPath/main.cpp -c $options
fi

# compile classes first
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating/Domain/ -I $srcPath  $srcPath/Heating/Common.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating//Domain/ -I $srcPath  $srcPath/Heating/Config.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating/Domain/ -I $srcPath  $srcPath/Heating/Domain/HeaterInfo.cpp -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating/Domain/ -I $srcPath  $srcPath/Heating/Domain/ZoneInfo.cpp -c $options

# compile test
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating/Domain/ -I $srcPath $testInclude $testPath/Domain/HeaterInfoTest.cpp  -c $options
g++ -I $mockPath -I $librariesPath/Time/ -I $srcPath/Heating/Domain/ -I $srcPath $testInclude $testPath/Domain/ZoneInfoTest.cpp  -c $options
g++ -I $mockPath -I $linkPath -I $linkPath/ArduinoJson/src -I $srcPath $testInclude $testPath/Heating/MqttParserTest.cpp  -c $options
g++ Arduino.o fake_serial.o WString.o Time.o Common.o Config.o HeaterInfo.o ZoneInfo.o  HeaterInfoTest.o ZoneInfoTest.o MqttParserTest.o main.o -o test
./test $1

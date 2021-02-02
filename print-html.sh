#!/bin/bash

html=$(cat index.html | sed ':a;N;$!ba;s/\n/\\n/g'  | sed 's/"/\\"/g')

echo "const char websiteContent[] PROGMEM {\"$html\"};" > master/html.h
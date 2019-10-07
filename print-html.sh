#!/bin/bash

cat index.html | sed ':a;N;$!ba;s/\n/\\n/g'  | sed 's/"/\\"/g'

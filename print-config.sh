#!/bin/bash

cat config.json | tr -d "\n" | tr -d " " | sed 's/"/\\"/g'

echo

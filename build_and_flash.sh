#!/bin/bash
set -e
cd react-app
npm install
npm run build:css
npm run build
cd ..
mkdir -p ./spiffs
cp -r ./react-app/build/* ./spiffs/
idf.py build
idf.py -p /dev/ttyUSB1 flash


#!/bin/bash
set -e
. ~/esp/esp-idf/export.sh
cd react-app
npm install
npm run build:css
# Set build timestamp environment variable
export REACT_APP_BUILD_TIME=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
echo "Building with timestamp: $REACT_APP_BUILD_TIME"
npm run build
cd ..
mkdir -p ./spiffs
rm -rf ./spiffs/*
cp -r ./react-app/build/* ./spiffs/
# Remove source maps and other debug files to save space
find ./spiffs -name "*.map" -delete
find ./spiffs -name "*.LICENSE.txt" -delete
idf.py build
idf.py -p /dev/ttyUSB1 flash


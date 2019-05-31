#!/bin/bash

echo doing make clean and make mrporper
make clean && make mrproper
echo doing rm -rf out
rm -rf out
echo done cleaning. now started compiling

#compile

./build.sh

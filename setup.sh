#!/bin/bash

mkdir -p app
mkdir -p app/.mcfw
mkdir -p app/.mcfw/lib
mkdir -p app/.mcfw/include
mkdir -p app/.mcfw/bin
mkdir -p app/.mcfw/crefl
cd submodules/asclib
make clean
make -j4 OPT=0
rm ../../dependencies/asclib/* -rf
cp -r headers/asclib ../../app/.mcfw/include
cp build/lib/libasclib.a ../../app/.mcfw/lib
cd ../crefl
make clean
rm ../../dependencies/crefl/* -rf
./extract.sh extracted
cp -r extracted/crefl_gen ../../app/.mcfw/lib
cp -r extracted/include/crefl ../../app/.mcfw/include
cp extracted/lib/libcrefl.a ../../app/.mcfw/lib
rm extracted -rf
make clean
cd ../../
make clean
make -j4

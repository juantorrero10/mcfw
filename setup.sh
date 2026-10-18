#!/bin/bash

rm dependencies -rf
mkdir dependencies
mkdir dependencies/asclib
mkdir dependencies/crefl
cd submodules/asclib
make clean
make -j4 OPT=0
rm ../../dependencies/asclib/* -rf
cp -r headers/asclib ../../dependencies
cp build/lib/libasclib.a ../../dependencies/asclib
cd ../crefl
make clean
rm ../../dependencies/crefl/* -rf
./extract.sh extracted
cp -r extracted/crefl_gen ../../dependencies/crefl
cp -r extracted/include/crefl ../../dependencies
cp extracted/lib/libcrefl.a ../../dependencies/crefl
rm extracted -rf
make clean
cd ../../
make clean
make -j4

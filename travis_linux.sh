#!/usr/bin/env bash
cd third_party/bullet
ls
../../tools/premake/premake5_linux gmake
cd build/linux
make config=debug
make config=release
cd ../../../../
cd examples
python3 ../tools/build.py -actions code -ide gmake -platform linux
cd build/linux
g++ -v
make config=debug
make config=release

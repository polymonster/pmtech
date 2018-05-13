#!/usr/bin/env bash
cd put/bullet
../../tools/premake/premake5_linux gmake
cd build
make config=debug
make config=release
cd examples
python3 ../tools/build.py -actions code -ide gmake -platform linux
cd build/linux
g++ -v
make config=debug
make config=release

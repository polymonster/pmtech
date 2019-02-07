#!/usr/bin/env bash
cd third_party
./build_libs.sh linux
cd ../examples
python3 ../tools/build.py -premake -ide gmake -platform linux
cd build/linux
g++ -v
make config=debug
make config=release

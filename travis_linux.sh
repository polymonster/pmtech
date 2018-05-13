#!/usr/bin/env bash
cd examples
python3 ../tools/build.py -actions code -ide gmake -platform linux
cd build/linux
g++ -v
make config=debug
make config=release

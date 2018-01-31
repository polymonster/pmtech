#!/usr/bin/env bash
cd examples
python3 ../tools/build.py -actions code -ide gmake -platform osx
cd build/osx
make config=debug
make config=release
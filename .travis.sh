#!/usr/bin/env bash
cd third_party
./build_libs.sh osx
cd ../examples
python3 ../tools/build.py -all -ide gmake -platform osx
cd build/osx
make config=debug
make config=release
cd ../..
python3 run_tests.py

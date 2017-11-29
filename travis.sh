#!/usr/bin/env bash
cd examples
python3 build.py -actions code -ide gmake -platform osx
cd build/osx
make Makefile all

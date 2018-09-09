#!/usr/bin/env bash

# third party build for osx, ios, linux

# bullet
cd bullet
../../tools/premake/premake5 gmake --platform_dir=$1
cd build/$1
make config=debug
make config=release
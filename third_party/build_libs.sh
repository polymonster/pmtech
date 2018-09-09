#!/usr/bin/env bash

# third party build for osx, ios, linux

# bullet
cd bullet
if [ "$1" = "linux" ]; then
    ../../tools/premake/premake5_linux gmake --platform_dir=$1
else
    ../../tools/premake/premake5 gmake --platform_dir=$1
fi

cd build/$1
make config=debug
make config=release
cd ../../..
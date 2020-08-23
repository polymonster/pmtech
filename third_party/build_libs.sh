#!/usr/bin/env bash

# third party build for osx, ios, linux

# bullet
cd bullet
if [ "$1" = "linux" ]; then
    ../../tools/premake/premake5_linux gmake --platform_dir=$1
    cd build/$1
	make config=debug
	make config=release
cd ../../..
elif [ "$1" = "ios" ]; then
    ../../tools/premake/premake5 xcode4 --xcode_target=ios --platform_dir=$1
    cd build/$1
    xcodebuild -scheme bullet_monolithic -destination generic/platform=iOS -project bullet_monolithic.xcodeproj -configuration Debug -quiet
    xcodebuild -scheme bullet_monolithic -destination generic/platform=iOS -project bullet_monolithic.xcodeproj -configuration Release -quiet
cd ../../..
elif [ "$1" = "web" ]; then
    ../../tools/premake/premake5 gmake --platform_dir=$1
    cd build/$1
    emmake make config=debug
    emmake make config=release
cd ../../..
elif [ "$1" = "mac" ]; then
    ../../tools/premake/premake5 gmake --platform_dir=$1
    cd build/$1
	make config=debug
	make config=release
cd ../../..
else
echo error unsupported target $1
fi

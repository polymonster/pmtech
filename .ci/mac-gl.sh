#!/usr/bin/env bash
cd examples
../pmbuild2 mac-gl-ci -libs -premake
cd build/linux
g++ -v
make config=debug
make config=release

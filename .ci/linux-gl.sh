#!/usr/bin/env bash
cd examples
../pmbuild linux -libs -premake
cd build/linux
g++ -v
make config=debug
make config=release

#!/usr/bin/env bash
cd examples
../pmbuild mac-metal-ci -libs -premake
cd build/osx
make config=debug
make config=release

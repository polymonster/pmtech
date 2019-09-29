#!/usr/bin/env bash
cd examples
../pmbuild mac-gl-ci -libs -premake
cd build/osx
make config=debug
make config=release

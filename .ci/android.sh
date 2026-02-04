#!/usr/bin/env bash
cd examples
../pmbuild android -libs
../pmbuild android
cd build/android
gradle wrapper
./gradlew assembleRelease

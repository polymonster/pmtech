#!/usr/bin/env bash
cd examples
../pmbuild linux -libs
../pmbuild linux
../pmbuild make linux all config=debug
../pmbuild make linux all config=release

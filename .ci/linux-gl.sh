#!/usr/bin/env bash
cd examples
../pmbuild linux -libs
../pmbuild linux
pmbuild make all config=debug
pmbuild make all config=release

#!/usr/bin/env bash
python3 -m pip install cryptography
cd examples
../pmbuild ios-ci -libs
../pmbuild ios-ci
../pmbuild make ios-ci all -destination generic/platform=iOS -configuration Debug CODE_SIGNING_ALLOWED=NO -quiet
../pmbuild make ios-ci all -destination generic/platform=iOS -configuration Release CODE_SIGNING_ALLOWED=NO -quiet
#!/usr/bin/env bash
python3 -m pip install cryptography
cd examples
../pmbuild ios -libs
../pmbuild ios
../pmbuild make ios all -destination generic/platform=iOS -configuration Debug CODE_SIGNING_ALLOWED=NO -quiet
../pmbuild make ios all -destination generic/platform=iOS -configuration Release CODE_SIGNING_ALLOWED=NO -quiet


#!/usr/bin/env bash
cd examples
../pmbuild ios -libs
../pmbuild ios
pmbuild make ios all -destination generic/platform=iOS -configuration Debug CODE_SIGNING_ALLOWED=NO
pmbuild make ios all -destination generic/platform=iOS -configuration Release CODE_SIGNING_ALLOWED=NO


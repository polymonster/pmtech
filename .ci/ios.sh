#!/usr/bin/env bash
cd examples
../pmbuild ios-ci -libs
../pmbuild ios-ci
../pmbuild make ios-ci all -destination generic/platform=iOS -configuration Release CODE_SIGNING_ALLOWED=NO
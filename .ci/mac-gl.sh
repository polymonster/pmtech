#!/usr/bin/env bash
python3 -m pip install cryptography
cd examples
../pmbuild mac-gl -libs
../pmbuild mac-gl
../pmbuild mac-gl make all -configuration Debug
../pmbuild mac-gl make all -configuration Release
#!/usr/bin/env bash
python3 -m pip install cryptography
cd examples
../pmbuild mac-gl -libs
../pmbuild mac-gl
../pmbuild make mac-gl all -configuration Debug
../pmbuild make mac-gl all -configuration Release
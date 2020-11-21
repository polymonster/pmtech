#!/usr/bin/env bash
python3 -m pip install cryptography
cd examples
../pmbuild mac
../pmbuild mac -libs
ls
../pmbuild mac
../pmbuild mac make all -configuration Debug
../pmbuild mac make all -configuration Release
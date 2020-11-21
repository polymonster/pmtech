#!/usr/bin/env bash
cd examples
../pmbuild mac -libs -all
../pmbuild mac make all -configuration Debug
../pmbuild mac make all -configuration Release

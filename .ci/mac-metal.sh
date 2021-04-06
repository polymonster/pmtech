#!/usr/bin/env bash
cd examples
../pmbuild mac -libs
../pmbuild mac
../pmbuild make mac all -configuration Release -quiet CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
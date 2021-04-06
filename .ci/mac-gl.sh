#!/usr/bin/env bash
cd examples
../pmbuild mac-gl -libs
../pmbuild mac-gl
../pmbuild make mac-gl all -configuration Release -quiet CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
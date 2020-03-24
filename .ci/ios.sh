#!/usr/bin/env bash
cd examples
../pmbuild ios-ci -libs -premake
cd build/ios
xcodebuild -scheme pen -destination generic/platform=iOS -workspace pmtech_examples_ios.xcworkspace -configuration Debug CODE_SIGNING_ALLOWED=NO
xcodebuild -scheme put -destination generic/platform=iOS -workspace pmtech_examples_ios.xcworkspace -configuration Debug CODE_SIGNING_ALLOWED=NO

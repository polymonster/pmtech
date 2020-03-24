#!/usr/bin/env bash
cd examples
../pmbuild ios-ci -libs -premake
cd build/ios
xcodebuild -scheme examples -destination generic/platform=iOS -workspace pmtech_examples_ios.xcworkspace -configuration Debug
xcodebuild -scheme examples -destination generic/platform=iOS -workspace pmtech_examples_ios.xcworkspace -configuration Release

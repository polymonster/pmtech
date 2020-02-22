#!/usr/bin/env bash
echo examples
find examples/code -name '**.cpp' -or -name '**.h' | xargs clang-format -style=file -i 
echo source
find source -name '**.inl' -or -name '**.h' | xargs clang-format -style=file -i 
find source -name '**.cpp' -or -name '*.mm' -or -name '*.c' | xargs clang-format -style=file -i 
echo pmfx
find assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/build_scripts/stub_format.py -tabs_to_spaces 4 -w -i 
find examples/assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/build_scripts/stub_format.py -tabs_to_spaces 4 -w -i 
find source -name '*.lua'| xargs python3 tools/build_scripts/stub_format.py -tabs_to_spaces 4 -w -i 
git add .
git commit -m "format-all"
git push
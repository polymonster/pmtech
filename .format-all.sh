#!/usr/bin/env bash
find examples/code -name '**.cpp' -or -name '**.h' | xargs clang-format -style=file -i 
find core -name '**.inl' -or -name '**.h' | xargs clang-format -style=file -i
find core -name '**.cpp' -or -name '*.mm' -or -name '*.c' | xargs clang-format -style=file -i
find assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/pmbuild_ext/stub_format.py -tabs_to_spaces 4 -w -i
find examples/assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/pmbuild_ext/stub_format.py -tabs_to_spaces 4 -w -i
find tools -name '*.lua'| xargs python3 tools/pmbuild_ext/stub_format.py -tabs_to_spaces 4 -w -i
find core -name '*.lua'| xargs python3 tools/pmbuild_ext/stub_format.py -tabs_to_spaces 4 -w -i
git add .
git commit -m "format-all"
git push

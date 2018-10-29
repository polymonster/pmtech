#!/usr/bin/env bash
find examples/code -name '**.cpp' -or -name '*.h' | xargs clang-format -style=file -i
find pen/include -name '**.inl' -or -name '*.h' | xargs clang-format -style=file -i
find pen/source -name '**.cpp' -or -name '*.h' -or -name '*mm' -or -name '.c' | xargs clang-format -style=file -i
find put/include -name '**.inl' -or -name '*.h' | xargs clang-format -style=file -i
find put/source -name '**.cpp' -or -name '*.h' | xargs clang-format -style=file -i
find assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/build_scripts/stub_format.py -tabs_to_spaces 4 -align_consecutive : -i
find examples/assets -name '*.jsn' -or -name '*.pmfx' | xargs python3 tools/build_scripts/stub_format.py -tabs_to_spaces 4 -align_consecutive : -i 
git add .
git commit -m "format-all"
git push

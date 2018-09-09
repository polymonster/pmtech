# third party build for win32

# bullet
cd bullet
../../tools/premake/premake5 vs2017 --platform_dir="win32"

cd build/win32
make config=debug
make config=release
cd ../../..
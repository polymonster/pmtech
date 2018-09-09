# third party build for win32

# bullet
cd bullet
../../tools/premake/premake5 vs2017 --platform_dir="win32"
cd build/win32
msbuild bullet_monolithic.sln /p:Configuration=Debug
msbuild bullet_monolithic.sln /p:Configuration=Release
cd ../../..
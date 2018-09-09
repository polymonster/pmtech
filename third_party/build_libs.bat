:: third party build for win32

:: bullet
cd bullet
..\..\tools\premake\premake5.exe vs2017 --platform_dir="win32"
cd build/vs2017
msbuild bullet_monolithic.sln /p:Configuration=Debug
msbuild bullet_monolithic.sln /p:Configuration=Release
cd ..\..\..
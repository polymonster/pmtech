:: third party build for win32
:: pushd .
:: cd /D "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"
:: call vcvarsall.bat x86_amd64
:: popd

:: bullet
cd bullet
..\..\tools\premake\premake5.exe vs2017 --platform_dir="win32"
cd build/vs2017
msbuild bullet_build.sln /p:Configuration=Debug
msbuild bullet_build.sln /p:Configuration=Release
cd ..\..\..
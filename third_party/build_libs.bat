:: third party build for win32
SET sdk=%3

:: build machines running from dev prompt dont require vcvarsall setup
IF [%1] == [] GOTO cibuild

pushd .
cd /D %1
call vcvarsall.bat x86_amd64
popd

cd bullet
..\..\tools\premake\premake5.exe vs2017 --platform_dir="win32" --pmtech_dir="%2" --sdk_version="%3"
GOTO build

:cibuild
cd bullet
..\..\tools\premake\premake5.exe vs2017 --platform_dir="win32"
GOTO build

:: bullet
:build
cd build/vs2017
msbuild bullet_build.sln /p:Configuration=Debug
msbuild bullet_build.sln /p:Configuration=Release
cd ..\..\..
platform_dir = "win32"
build_cmd = ""
link_cmd = ""
if _ACTION == "xcode4" 
then 
platform_dir = "osx" 
build_cmd = "-std=c++11 -stdlib=libc++"
link_cmd = "-stdlib=libc++"
end

-- Solution
solution "Examples"
	location "build"
	configurations { "Debug", "Release" }
	startproject "basic_triangle"
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	
-- Engine Project	
dofile "..//PEN//project.lua"

-- Toolkit Project	
dofile "..//PUT//project.lua"

-- Project	
project "basic_triangle"
	location "build"
	kind "WindowedApp"
	language "C++"
	files { "source\\basic_triangle.cpp" }
	
	libdirs
	{ 
		"..\\PEN\\lib\\" .. platform_dir, 
		"..\\PEN\\third_party\\fmod\\lib",
		"..\\PEN\\third_party\\bullet\\lib",
		"..\\PEN\\..\\PUT\\lib\\" .. platform_dir
	}
	
	includedirs
	{ 
		"..\\PEN\\include", 
		"..\\PEN\\include\\" .. platform_dir,
		"..\\PEN\\..\\PUT\\include\\" .. platform_dir,
		"..\\PEN\\..\\PUT\\include\\",
		"include\\"
	}
	
	links { "d3d11.lib", "dxguid.lib", "winmm.lib", "comctl32.lib", "fmodex_vc.lib", "pen", "put" }
 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain", "Symbols" }
		targetdir ("bin\\" .. platform_dir)
		debugdir ("bin\\" .. platform_dir)
		targetname "app_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "Optimize" }
		targetdir ("bin\\" .. platform_dir)
		debugdir ("bin\\" .. platform_dir)
		targetname "app"
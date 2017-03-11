-- win32

-- Solution
solution "Examples"
	location "build"
	configurations { "Debug", "Release" }
	startproject "basic_triangle"
	
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
		os.getenv("PEN_DIR") .. "\\lib\\win32", 
		os.getenv("PEN_DIR") .. "\\third_party\\fmod\\lib",
		os.getenv("PEN_DIR") .. "\\third_party\\bullet\\lib",
		os.getenv("PEN_DIR") .. "\\..\\PUT\\lib\\win32"
	}
	
	includedirs
	{ 
		os.getenv("PEN_DIR") .. "\\include", 
		os.getenv("PEN_DIR") .. "\\include\\win32",
		os.getenv("PEN_DIR") .. "\\..\\PUT\\include\\win32",
		os.getenv("PEN_DIR") .. "\\..\\PUT\\include\\",
		"include\\"
	}
	
	links { "d3d11.lib", "dxguid.lib", "winmm.lib", "comctl32.lib", "fmodex_vc.lib", "pen", "put" }
 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain", "Symbols" }
		targetdir "bin\\win32"
		debugdir "bin\\win32"
		targetname "app_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "Optimize" }
		targetdir "bin\\win32"
		debugdir "bin\\win32"
		targetname "app"
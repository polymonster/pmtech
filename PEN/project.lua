-- Project	
project "pen"
	location "build\\win32"
	kind "StaticLib"
	language "C++"
	files { "include\\*.h", "include\\win32\\**.h", "source\\win32\\**.cpp" }
	includedirs { "include", "include\\win32", "third_party\\fmod\\inc" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain", "Symbols" }
		targetdir "lib\\win32"
		targetname "pen_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "Optimize" }
		targetdir "lib\\win32"
		targetname "pen"
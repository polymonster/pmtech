-- Project	
project "pen"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	files 
	{ 
		"include\\*.h", 
		"include\\" .. platform_dir .. "\\**.h", 
		"source\\" .. platform_dir .. "\\**.cpp" 
	}
	
	includedirs 
	{ 
		"include", 
		"include\\" .. platform_dir, 
		"third_party\\fmod\\inc" 
	}
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain", "Symbols" }
		targetdir ("lib\\" .. platform_dir)
		targetname "pen_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "Optimize" }
		targetdir ("lib\\" .. platform_dir)
		targetname "pen"
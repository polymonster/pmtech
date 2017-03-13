-- Project	
project "put"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 
		"..\\PEN\\lib\\" .. platform_dir,
		"..\\PEN\\third_party\\bullet\\lib",
	}
	
	includedirs
	{ 
		"..\\PEN\\include", 
		"..\\PEN\\include\\" .. platform_dir,
		"..\\PEN\\include\\" .. renderer_dir,  
		"..\\PEN\\third_party\\bullet\\include"
	}
		
	files { "include\\**.h", "source\\**.cpp" }
	includedirs { "include" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib\\" .. platform_dir)
		targetname "put_d"
		links { "bullet_monolithic_do.lib" }
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib\\" .. platform_dir)
		targetname "put"
		links { "bullet_monolithic.lib" }
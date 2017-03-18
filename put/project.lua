-- Project	
project "put"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 
		"../pen/lib/" .. platform_dir,
		
		"../pen/third_party/bullet/lib",
	}
	
	includedirs
	{ 
		"..\\pen\\include\\common", 
		"..\\pen\\include\\" .. platform_dir,
		"..\\pen\\include\\" .. renderer_dir,
		  
		"..\\pen\\third_party\\bullet\\include"
	}
		
	files { "include\\**.h", "source\\**.cpp" }
	includedirs { "include" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "put_d"
		links { "bullet_monolithic_do.lib" }
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib/" .. platform_dir)
		targetname "put"
		links { "bullet_monolithic.lib" }
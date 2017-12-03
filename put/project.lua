-- Project	
project "put"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 
		"../pen/lib/" .. platform_dir,
		
		"../put/bullet/lib/" .. platform_dir,
	}
	
	includedirs
	{ 
		"..\\pen\\include\\common", 
		"..\\pen\\include\\" .. platform_dir,
		"..\\pen\\include\\" .. renderer_dir,
		  
		"..\\put\\bullet\\src\\",
		"..\\pen\\third_party\\imgui",
		"..\\pen\\third_party",
	}
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" then
		systemversion("8.1:10.1")
	end
		
	files 
	{ 
		"include\\**.h", "source\\**.cpp", 
		
		"..\\pen\\third_party\\imgui\\*.cpp",
		"..\\pen\\third_party\\imgui\\*.h",
	}
	includedirs { "include" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "put_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib/" .. platform_dir)
		targetname "put"
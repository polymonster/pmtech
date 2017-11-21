-- Project	
project "pen"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	files 
	{ 
		"include/common/**.h",
		"source/common/**.cpp",
		 
		"include/" .. platform_dir .. "/**.h", 
		"include/" .. renderer_dir .. "/**.h",
		
		"source/" .. renderer_dir .. "/**.cpp",
		
		"source/" .. platform_dir .. "/**.cpp",
		"source/" .. platform_dir .. "/**.mm",
		
		"third_party/str/*.cpp", 
	}
	
	if platform_dir == "osx" then
	files 
	{  
		"source/posix/**.cpp"
	}
	end
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" then
		systemversion("8.1:10.1")
	end
	
	includedirs 
	{ 
		"include/common",
		 
		"include/" .. platform_dir, 
		"include/" .. renderer_dir,
		
		"third_party/fmod/inc",
		
		"third_party" 
	}
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib\\" .. platform_dir)
		targetname "pen_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib\\" .. platform_dir)
		targetname "pen"
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
	}
	
	if platform_dir == "osx" then
	files 
	{  
		"source/posix/**.cpp"
	}
	end

	if _ACTION == "vs2017" then
	systemversion "10.0.14393.0"
	end
	
	includedirs 
	{ 
		"include/common",
		 
		"include/" .. platform_dir, 
		"include/" .. renderer_dir,
		
		"third_party/fmod/inc" 
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
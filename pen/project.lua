-- Project	
project "pen"
	location ("build/" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	files 
	{
		"include/*.h",
		"source/*.cpp",
		 
		"include/" .. platform_dir .. "/**.h", 
		"include/" .. renderer_dir .. "/**.h",
		
		"source/" .. renderer_dir .. "/**.cpp",
		
		"source/" .. platform_dir .. "/**.cpp",
		"source/" .. platform_dir .. "/**.mm",
		
		"../third_party/str/*.cpp", 
	}
	
	if platform_dir == "osx" then
	files 
	{  
		"source/posix/**.cpp"
	}
	end

	if platform_dir == "linux" then
	files 
	{  
		"source/posix/**.cpp"
	}
	end
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" then
		systemversion(windows_sdk_version())
		disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
	end
	
	includedirs 
	{
		"include",

		"include/" .. platform_dir, 
		"include/" .. renderer_dir,
		
		"../third_party/fmod/inc",
		"../third_party" 
	}
			
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "pen_d"
		architecture "x64"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib/" .. platform_dir)
		targetname "pen"
		architecture "x64"

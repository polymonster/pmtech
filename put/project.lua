bullet_lib_dir = "osx"
if _ACTION == "vs2017" or _ACTION == "vs2015" then
	bullet_lib_dir = _ACTION
end
-- Project	
project "put"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 
		"../pen/lib/" .. platform_dir,
		
		"../put/bullet/lib/" .. bullet_lib_dir,
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
		systemversion("10.0:10.1")
	end
			
	files 
	{ 
		"include\\**.h", "source\\**.cpp", 
		
		"..\\pen\\third_party\\imgui\\*.cpp",
		"..\\pen\\third_party\\imgui\\*.h",
	}
	includedirs { "include" }
	disablewarnings { "4800", "4305", "4018" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		links { "bullet_monolithic_d" }
		targetname "put_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		links { "bullet_monolithic" }
		targetdir ("lib/" .. platform_dir)
		targetname "put"
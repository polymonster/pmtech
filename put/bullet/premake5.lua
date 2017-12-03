platform_dir = "osx"
if _ACTION == "vs2017" or _ACTION == "vs2015" then
	platform_dir = "_ACTION"
end

solution "bullet_build"
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	
-- Project	
project "bullet_monolithic"
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 

	}
	
	includedirs
	{ 
		"src\\", 
	}
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" then
		systemversion("8.1:10.1")
	end
		
	files 
	{ 
		"src\\Bullet3Collision\\**.*",
		"src\\Bullet3Common\\**.*",
		"src\\Bullet3Dynamics\\**.*", 
		"src\\Bullet3Geometry\\**.*", 
		"src\\Bullet3Serialize\\**.*", 
		"src\\LinearMath\\**.*", 
	}
	
	includedirs { "include" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic"
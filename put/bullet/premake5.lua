platform_dir = "linux"
if _ACTION == "vs2017" or _ACTION == "vs2015" then
	platform_dir = _ACTION
end

link_cmd = ""
if platform_dir == "osx" then
	link_cmd = "-mmacosx-version-min=10.8"
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
		"src\\BulletDynamics\\**.*", 
		"src\\BulletCollision\\**.*", 
		"src\\LinearMath\\**.*", 
	}
	
	includedirs { "include" }
				
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		linkoptions { link_cmd }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic_d"
		architecture "x64"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		linkoptions { link_cmd }
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic"
		architecture "x64"

dofile("../../tools/premake/options.lua")
dofile("../../tools/premake/globals.lua")

link_cmd = ""
build_cmd = ""
if platform_dir == "osx" then
	link_cmd = "-mmacosx-version-min=10.8"
elseif platform == "web" then
	build_cmd = "-std=c++11 -s USE_PTHREADS=1"
	link_cmd = "-s USE_PTHREADS=1 -s FULL_ES3=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s PTHREAD_POOL_SIZE=8"
end

solution "bullet_build"
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	
-- Project	
project "bullet_monolithic"
	setup_env()
	location ("build\\" .. platform_dir)
	kind "StaticLib"
	language "C++"
	
	includedirs
	{ 
		"src\\", 
	}
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" or ACTION == "vs2019" then
		systemversion(windows_sdk_version())
		disablewarnings { "4267", "4305", "4244" }
	end
	
	setup_env()
	
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
		entrypoint "WinMainCRTStartup"
		linkoptions { link_cmd }
		buildoptions { build_cmd }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		entrypoint "WinMainCRTStartup"
		optimize "Speed"
		linkoptions { link_cmd }
		buildoptions { build_cmd }
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic"

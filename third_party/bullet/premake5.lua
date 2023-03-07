dofile("../../tools/premake/options.lua")
dofile("../../tools/premake/globals.lua")

link_cmd = ""
build_cmd = ""
if platform_dir == "osx" then
	xcodebuildsettings {
		["MACOSX_DEPLOYMENT_TARGET"] = "10.13"
	}
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
	
	includedirs { 
		"src\\", 
	}
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" or _ACTION == "vs2019" or _ACTION == "vs2022" then
		systemversion(windows_sdk_version())
		disablewarnings { "4267", "4305", "4244" }
	end
	
	setup_env()
	
	files { 
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
				
	filter "configurations:Debug"
		defines { "DEBUG" }
		entrypoint "WinMainCRTStartup"
		linkoptions { link_cmd }
		buildoptions { build_cmd }
		symbols "On"
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic_d"
 
	filter "configurations:Release"
		defines { "NDEBUG" }
		entrypoint "WinMainCRTStartup"
		optimize "Speed"
		linkoptions { link_cmd }
		buildoptions { build_cmd }
		targetdir ("lib/" .. platform_dir)
		targetname "bullet_monolithic"

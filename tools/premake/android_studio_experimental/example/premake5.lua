require "android_studio_experimental"

workspace "android_studio_example"
	configurations { "Debug", "Release" }
	gradleversion "com.android.tools.build:gradle:3.1.4"
	location ("build")

project "android_studio_example"
	kind "ConsoleApp"
	language "C"
	targetdir "bin/%{cfg.buildcfg}"

	files 
	{ 
		"cpp/**.*", 
		"java/**.*" 
	}

	configuration "Debug"
		defines { "DEBUG" }
		symbols "On"

	configuration "Release"
		defines { "NDEBUG" }
		optimize "On"

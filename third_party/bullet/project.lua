project "bullet_monolithic"
	location ("build/" .. platform_dir)
	kind "StaticLib"
	language "C++"

	includedirs {
		"../bullet/src/",
		"../bullet/include",
	}

	if _ACTION == "vs2017" or _ACTION == "vs2015" or _ACTION == "vs2019" or _ACTION == "vs2022" then
		systemversion(windows_sdk_version())
		disablewarnings { "4267", "4305", "4244" }
	end

	if platform == "android" then
		buildoptions {
			"-Wno-unused-variable",
			"-Wno-unused-parameter",
			"-Wno-sign-compare",
			"-Wno-reorder",
		}
	end

	files {
		"../bullet/src/Bullet3Collision/**.*",
		"../bullet/src/Bullet3Common/**.*",
		"../bullet/src/Bullet3Dynamics/**.*",
		"../bullet/src/Bullet3Geometry/**.*",
		"../bullet/src/Bullet3Serialize/**.*",
		"../bullet/src/BulletDynamics/**.*",
		"../bullet/src/BulletCollision/**.*",
		"../bullet/src/LinearMath/**.*",
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		targetdir ("../bullet/lib/" .. platform_dir)
		targetname "bullet_monolithic_d"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "Speed"
		targetdir ("../bullet/lib/" .. platform_dir)
		targetname "bullet_monolithic"

	filter {}

function create_app( project_name, root_directory )
project ( project_name )
	kind "WindowedApp"
	language "C++"
	
	libdirs
	{ 
		"..\\PEN\\lib\\" .. platform_dir, 
		"..\\PEN\\third_party\\fmod\\lib",
		"..\\PEN\\third_party\\bullet\\lib",
		"..\\PEN\\..\\PUT\\lib\\" .. platform_dir
	}
	
	includedirs
	{ 
		"..\\PEN\\include", 
		"..\\PEN\\include\\" .. platform_dir,
		"..\\PEN\\include\\" .. renderer_dir,
		"include\\"
	}
	
	location ( root_directory .. "/build" )
	targetdir ( root_directory .. "/bin/" .. platform_dir )
	debugdir ( root_directory .. "/bin/" .. platform_dir)
	
	if platform_dir == "win32" then 
		links 
		{ 
			"d3d11.lib", 
			"dxguid.lib", 
			"winmm.lib", 
			"comctl32.lib", 
			"fmodex_vc.lib", 
			"pen"
		}
	elseif platform_dir == "osx" then
		links 
		{ 
			"pen"
		}
	end

	files 
	{ 
		(root_directory .. "/" .. project_name .. "/*.cpp"),
		(root_directory .. "/" .. project_name .. "/*.c"),
		(root_directory .. "/" .. project_name .. "/*.h"),
	}
 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetname "app_d"
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetname "app"
end

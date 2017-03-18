function create_app( project_name, root_directory )
project ( project_name )
	kind "WindowedApp"
	language "C++"
	
	libdirs
	{ 
		"../pen/lib/" .. platform_dir, 
		"../put/lib/" .. platform_dir,
				
		"../pen/third_party/fmod/lib",
		"../pen/third_party/bullet/lib",
	}
	
	includedirs
	{ 
		"..\\pen\\include\\common", 
		"..\\pen\\include\\" .. platform_dir,
		"..\\pen\\include\\" .. renderer_dir,
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
		}
	elseif platform_dir == "osx" then
		links 
		{ 
			"pen",
			"Cocoa.framework",
			"OpenGL.framework",
			"iconv"
		}
	end

	files 
	{ 
		(root_directory .. "/" .. project_name .. "/*.cpp"),
		(root_directory .. "/" .. project_name .. "/*.c"),
		(root_directory .. "/" .. project_name .. "/*.h"),
		(root_directory .. "/" .. project_name .. "/*.m")
	}
 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetname (project_name .. "_d")
		links { "pen_d" }
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetname (project_name)
		links { "pen" }
end

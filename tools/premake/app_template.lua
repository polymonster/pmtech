function add_osx_links()
links 
{ 
	"Cocoa.framework",
	"OpenGL.framework",
	"iconv"
}
end

function add_win32_links()
links 
{ 
	"d3d11.lib", 
	"dxguid.lib", 
	"winmm.lib", 
	"comctl32.lib", 
	"fmodex_vc.lib", 
}
end

function add_ios_links()
links 
{ 
	"OpenGLES.framework",
	"Foundation.framework",
	"UIKit.framework",
	"GLKit.framework",
	"QuartzCore.framework"
}
end

function add_ios_files( project_name, root_directory )
files 
{ 
	root_directory .. "/" .. project_name .. "/ios_files/**.*"
}

excludes 
{ 
	root_directory .. "/" .. project_name .. "**.DS_Store"
}
end

function create_app( project_name, root_directory, link_put )
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
		"../pen/include/common", 
		"../pen/include/" .. platform_dir,
		"../pen/include/" .. renderer_dir,
		"../put/include/",
		
		"include/"
	}
	
	location ( root_directory .. "/build/" .. platform_dir )
	targetdir ( root_directory .. "/bin/" .. platform_dir )
	debugdir ( root_directory .. "/bin/" .. platform_dir)
	
	if platform_dir == "win32" then add_win32_links()
	elseif platform_dir == "osx" then add_osx_links()
	elseif platform_dir == "ios" then 
		add_ios_links() 
		add_ios_files( project_name, root_directory )
	end

	files 
	{ 
		(root_directory .. "code/" .. project_name .. "/**.cpp"),
		(root_directory .. "code/" .. project_name .. "/**.c"),
		(root_directory .. "code/" .. project_name .. "/**.h"),
		(root_directory .. "code/" .. project_name .. "/**.m"),
	}
	 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetname (project_name .. "_d")
		links { "pen_d" }
		
		if link_put then links { "put_d" } end
  
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetname (project_name)
		links { "pen" }
		
		if link_put then links { "put_d" } end
end

function add_osx_links()
links 
{ 
	"Cocoa.framework",
	"OpenGL.framework",
	"GameController.framework",
	"iconv",
	"fmod"
}
end

function add_linux_links()
links 
{ 
	"pthread",
	"GLEW",
	"GLU",
	"GL",
	"X11",
	"fmod"
}
end

function add_win32_links()
links 
{ 
	"d3d11.lib", 
	"dxguid.lib", 
	"winmm.lib", 
	"comctl32.lib", 
	"fmod64_vc.lib",
	"Shlwapi.lib"	
}
end

function add_ios_links()
links 
{ 
	"OpenGLES.framework",
	"Foundation.framework",
	"UIKit.framework",
	"GLKit.framework",
	"QuartzCore.framework",
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

bullet_lib = "bullet_monolithic"
bullet_lib_debug = "bullet_monolithic_d"
bullet_lib_dir = "osx"

if platform_dir == "linux" then
	bullet_lib_dir = "linux"
end

if _ACTION == "vs2017" or _ACTION == "vs2015" then
	bullet_lib_dir = _ACTION
	bullet_lib = (bullet_lib .. "_x64")
	bullet_lib_debug = (bullet_lib_debug .. "_x64")
end

function create_app( project_name, source_directory, root_directory )
project ( project_name )
	kind "WindowedApp"
	language "C++"
	
	libdirs
	{ 
		pmtech_dir .. "pen/lib/" .. platform_dir, 
		pmtech_dir .. "put/lib/" .. platform_dir,
				
		(pmtech_dir .. "third_party/fmod/lib/" .. platform_dir),
		(pmtech_dir .. "third_party/bullet/lib/" .. bullet_lib_dir),
	}
	
	includedirs
	{
		pmtech_dir .. "pen/include",
		pmtech_dir .. "pen/include/common", 
		pmtech_dir .. "pen/include/" .. platform_dir,
		pmtech_dir .. "pen/include/" .. renderer_dir,
		pmtech_dir .. "put/include/",
		pmtech_dir .. "third_party/",
		
		"include/"
	}
	
	if _ACTION == "vs2017" or _ACTION == "vs2015" then
		systemversion(windows_sdk_version())
		disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
	end
			
	location ( root_directory .. "/build/" .. platform_dir )
	targetdir ( root_directory .. "/bin/" .. platform_dir )
	debugdir ( root_directory .. "/bin/" .. platform_dir)
	
	if platform_dir == "win32" then add_win32_links()
	elseif platform_dir == "osx" then add_osx_links()
	elseif platform_dir == "ios" then 
		add_ios_links() 
		add_ios_files( project_name, root_directory )
	elseif platform_dir == "linux" then add_linux_links()
	end
	
	files 
	{ 
		(root_directory .. "code/" .. source_directory .. "/**.cpp"),
		(root_directory .. "code/" .. source_directory .. "/**.c"),
		(root_directory .. "code/" .. source_directory .. "/**.h"),
		(root_directory .. "code/" .. source_directory .. "/**.m"),
	}
		 
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetname (project_name .. "_d")
		links { "put", "pen", bullet_lib_debug }
		architecture "x64"
  
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetname (project_name)
		links { "put", "pen", bullet_lib }
		architecture "x64"
		
end

function create_app_example( project_name, root_directory )
	create_app( project_name, project_name, root_directory )
end

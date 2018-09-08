function add_gnu_make_links()
if _ACTION == "gmake"
	
	configuration "Debug"
		links { "pen_d", "put_d" }
	
	configuration "Release"
		links { "pen", "put" }
		
	configuration {}
end
end


local function add_osx_links()
links 
{ 
	"Cocoa.framework",
	"OpenGL.framework",
	"GameController.framework",
	"iconv",
	"fmod"
}
add_gnu_make_links()
end

local function add_linux_links()
links 
{ 
	"pthread",
	"GLEW",
	"GLU",
	"GL",
	"X11",
	"fmod"
}
add_gnu_make_links()
end

local function add_win32_links()
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

local function add_ios_links()
links 
{ 
	"OpenGLES.framework",
	"Foundation.framework",
	"UIKit.framework",
	"GLKit.framework",
	"QuartzCore.framework",
}
end

local function add_ios_files(project_name, root_directory)
files 
{ 
	(pmtech_dir .. "/template/ios/**.*"),
	"bin/ios/data"
}

excludes 
{ 
	("**.DS_Store")
}

xcodebuildresources
{
	"bin/ios/data"
}
end

local function setup_android()
	files
	{
		pmtech_dir .. "/template/android/manifest/**.*",
		pmtech_dir .. "/template/android/activity/**.*"
	}
	
	androidabis
	{
		"armeabi-v7a", "x86"
	}
end

local function setup_platform(project_name, root_directory)
	if platform_dir == "win32" then
		systemversion(windows_sdk_version())
		disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" } 
		add_win32_links()
	elseif platform_dir == "osx" then
		add_osx_links()
	elseif platform_dir == "ios" then
		add_ios_links() 
		add_ios_files(project_name, root_directory)
	elseif platform_dir == "linux" then 
		add_linux_links()
	elseif platform_dir == "android" then 
		setup_android()
	end
end

local function setup_bullet()
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
	
	libdirs
	{
		(pmtech_dir .. "third_party/bullet/lib/" .. bullet_lib_dir)
	}
	
	configuration "Debug"
		links { bullet_lib_debug }
	
	configuration "Release"
		links { bullet_lib }
	
	configuration {}
end

local function setup_fmod()
	libdirs
	{
		(pmtech_dir .. "third_party/fmod/lib/" .. platform_dir)
	}
end

function setup_modules()
	setup_bullet()
	setup_fmod()
end

function create_app(project_name, source_directory, root_directory)
project ( project_name )
	kind "WindowedApp"
	language "C++"
	dependson { "pen", "put" }
		
	includedirs
	{
		pmtech_dir .. "pen/include",
		pmtech_dir .. "pen/include/common", 
		pmtech_dir .. "pen/include/" .. platform_dir,
		pmtech_dir .. "pen/include/" .. renderer_dir,
		pmtech_dir .. "put/include/",
		pmtech_dir .. "third_party/",
		
		"include/",
	}
		
	files 
	{ 
		(root_directory .. "code/" .. source_directory .. "/**.cpp"),
		(root_directory .. "code/" .. source_directory .. "/**.c"),
		(root_directory .. "code/" .. source_directory .. "/**.h"),
		(root_directory .. "code/" .. source_directory .. "/**.m"),
		(root_directory .. "code/" .. source_directory .. "/**.mm")
	}
	
	libdirs
	{ 
		pmtech_dir .. "pen/lib/" .. platform_dir, 
		pmtech_dir .. "put/lib/" .. platform_dir,
	}
	
	setup_env()
	setup_platform(project_name, root_directory)
	setup_modules()
	
	location (root_directory .. "/build/" .. platform_dir)
	targetdir (root_directory .. "/bin/" .. platform_dir)
	debugdir (root_directory .. "/bin/" .. platform_dir)
	
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain" }
		symbols "On"
		targetname (project_name .. "_d")
		architecture "x64"
  
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "OptimizeSpeed" }
		targetname (project_name)
		architecture "x64"
		
end

function create_app_example( project_name, root_directory )
	create_app( project_name, project_name, root_directory )
end

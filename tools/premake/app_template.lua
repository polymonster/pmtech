local function add_pmtech_links()
	configuration "Debug"
		links { "put_d", "pen_d" }

	configuration "Release"
		links {  "put", "pen" }
	
	configuration {}
end

local function copy_shared_libs()
	configuration "Debug"
		postbuildcommands 
		{
			("{COPY} " .. shared_libs_dir .. " %{cfg.targetdir}")
		}
		
	configuration "Release"
		postbuildcommands 
		{
			("{COPY} " .. shared_libs_dir .. " %{cfg.targetdir}")
		}
	
	configuration {}
end

local function setup_osx()
	links 
	{ 
		"Cocoa.framework",
		"GameController.framework",
		"iconv",
		"fmod",
		"IOKit.framework"
	}
	
	if renderer_dir == "metal" then
		links 
		{ 
			"MetalKit.framework",
			"Metal.framework"
		}
	elseif renderer_dir == "opengl" then
		links 
		{ 
			"OpenGL.framework"
		}
	end
	
	-- add_pmtech_links()
	copy_shared_libs()
end

local function setup_linux()
	--linux must be linked in order
	add_pmtech_links()
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

local function setup_win32()
	links 
	{ 
		"d3d11.lib", 
		"dxguid.lib", 
		"winmm.lib", 
		"comctl32.lib", 
		"fmod64_vc.lib",
		"Shlwapi.lib"	
	}
	add_pmtech_links()
	
	systemversion(windows_sdk_version())
	disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }

	copy_shared_libs()
end

local function setup_ios()
	links 
	{ 
		"OpenGLES.framework",
		"Foundation.framework",
		"UIKit.framework",
		"GLKit.framework",
		"QuartzCore.framework",
	}
	
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

local function setup_platform()
	if platform_dir == "win32" then
		setup_win32()
	elseif platform_dir == "osx" then
		setup_osx()
	elseif platform_dir == "ios" then
		setup_ios()
	elseif platform_dir == "linux" then 
		setup_linux()
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
		bullet_lib = (bullet_lib)
		bullet_lib_debug = (bullet_lib_debug)
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
		dependson{ "pen", "put" }
		
		includedirs
		{
			-- core
			pmtech_dir .. "source/pen/include",
			pmtech_dir .. "source/pen/include/common", 
			pmtech_dir .. "source/pen/include/" .. platform_dir,
			pmtech_dir .. "source/pen/include/" .. renderer_dir,
			
			--utility			
			pmtech_dir .. "source/put/source/",
			
			-- third party			
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
		
		setup_env()
		setup_platform()
		setup_modules()
	
		location (root_directory .. "/build/" .. platform_dir)
		targetdir (root_directory .. "/bin/" .. platform_dir)
		debugdir (root_directory .. "/bin/" .. platform_dir)
						
		configuration "Release"
			defines { "NDEBUG" }
			flags { "WinMain", "OptimizeSpeed" }
			targetname (project_name)
			architecture "x64"
			libdirs
			{ 
				pmtech_dir .. "source/pen/lib/" .. platform_dir .. "/debug", 
				pmtech_dir .. "source/put/lib/" .. platform_dir .. "/debug",
			}
		
		configuration "Debug"
			defines { "DEBUG" }
			flags { "WinMain" }
			symbols "On"
			targetname (project_name .. "_d")
			architecture "x64"
			libdirs
			{ 
				pmtech_dir .. "source/pen/lib/" .. platform_dir .. "/release", 
				pmtech_dir .. "source/put/lib/" .. platform_dir .. "/release",
			}
		
end

function create_app_example( project_name, root_directory )
	create_app( project_name, project_name, root_directory )
end

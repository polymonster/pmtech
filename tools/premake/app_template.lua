local function add_pmtech_links()
	configuration "Debug"
		links { "put", "pen" }

	configuration "Release"
		links {  "put", "pen" }
	
	configuration {}
end

local function setup_osx()
	links 
	{ 
		"Cocoa.framework",
		"GameController.framework",
		"iconv",
		"fmod",
		"IOKit.framework",
		"MetalKit.framework",
		"Metal.framework",
		"OpenGL.framework"
	}
	add_pmtech_links()
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
		"fmod",
		"dl"
	}
end

local function setup_win32()
    if renderer_dir == "vulkan" then
        libdirs
        {
            "$(VK_SDK_PATH)/Lib"
        }
        links
        {
            "vulkan-1.lib"
        }
    elseif renderer_dir == "opengl" then
        includedirs
        {
            pmtech_dir .. "/third_party/glew/include"
        }
        libdirs
        {
            pmtech_dir .. "/third_party/glew/lib/win64"
        }
		links 
        { 
            "OpenGL32.lib"
        }
    else
    	links 
        { 
            "d3d11.lib"
        }
    end
    
    links
    {
        "dxguid.lib",
        "winmm.lib", 
        "comctl32.lib", 
        "fmod64_vc.lib",
        "Shlwapi.lib"	
    }

	add_pmtech_links()
	
	systemversion(windows_sdk_version())
	disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
end

local function setup_ios()
	links 
	{ 
		"Foundation.framework",
		"UIKit.framework",
		"QuartzCore.framework",
		"MetalKit.framework",
		"Metal.framework",
		"AVFoundation.framework",
		"AudioToolbox.framework",
		"MediaPlayer.framework",
		"fmod_iphoneos"
	}
		
	files 
	{ 
		(pmtech_dir .. "/core/template/ios/**.*"),
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
		pmtech_dir .. "/core/template/android/manifest/**.*",
		pmtech_dir .. "/core/template/android/activity/**.*"
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
	libdirs
	{
		(pmtech_dir .. "third_party/bullet/lib/" .. platform_dir)
	}
	
	configuration "Debug"
		links { "bullet_monolithic_d" }
	
	configuration "Release"
		links { "bullet_monolithic" }
	
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


function create_binary(project_name, source_directory, root_directory, binary_type)
	project ( project_name )
		setup_product( project_name )
		kind ( binary_type )
		language "C++"
		
		if binary_type ~= "SharedLib" then
			dependson { "pen", "put" }
		end
	
		includedirs
		{
			-- platform
			pmtech_dir .. "core/pen/include",
			pmtech_dir .. "core/pen/include/common", 
			pmtech_dir .. "core/pen/include/" .. platform_dir,
		
			--utility			
			pmtech_dir .. "core/put/source/",
		
			-- third party			
			pmtech_dir .. "third_party/",
	
			-- local
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
		setup_platform_defines()
		setup_modules()

		location (root_directory .. "/build/" .. platform_dir)
		targetdir (root_directory .. "/bin/" .. platform_dir)
		debugdir (root_directory .. "/bin/" .. platform_dir)
					
		configuration "Release"
			defines { "NDEBUG" }
			entrypoint "WinMainCRTStartup"
			optimize "Speed"
			targetname (project_name)
			libdirs
			{ 
				pmtech_dir .. "core/pen/lib/" .. platform_dir .. "/release", 
				pmtech_dir .. "core/put/lib/" .. platform_dir .. "/release",
			}
	
		configuration "Debug"
			defines { "DEBUG" }
			entrypoint "WinMainCRTStartup"
			symbols "On"
			targetname (project_name .. "_d")
			libdirs
			{ 
				pmtech_dir .. "core/pen/lib/" .. platform_dir .. "/debug", 
				pmtech_dir .. "core/put/lib/" .. platform_dir .. "/debug",
			}
end

function create_app(project_name, source_directory, root_directory)
	create_binary(project_name, source_directory, root_directory, "WindowedApp")
end

function create_app_example( project_name, root_directory )
	create_app( project_name, project_name, root_directory )
end

local s_project_name

local function add_pmtech_links()
	filter "configurations:Debug"
		links { "put", "pen" }

	filter "configurations:Release"
		links {  "put", "pen" }

	filter {}
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

	if _OPTIONS["plist"] then
		files
		{
			_OPTIONS["plist"]
		}
	else
		files
		{
			(pmtech_dir .. "core/template/osx/Info.plist")
		}
	end

	if _ACTION == "xcode4" then
	install_name_tool = "cd ../../bin/osx && install_name_tool -add_rpath @executable_path/../../.. "
	filter "configurations:Debug"
		postbuildcommands {
			install_name_tool .. s_project_name .. "_d.app/Contents/MacOS/" .. s_project_name .. "_d" .. " || true"
		}
	filter "configurations:Release"
		postbuildcommands {
			install_name_tool .. s_project_name .. ".app/Contents/MacOS/" .. s_project_name .. " || true"
		}
	filter {}
	end
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

	linkoptions {
		'-Wl,-rpath=\\$$ORIGIN',
		"-export-dynamic"
	}
end

local function setup_win32()
    if renderer_dir == "vulkan" then
        libdirs
        {
            "$(VK_SDK_PATH)/Lib"
        }
        includedirs
        {
            "$(VK_SDK_PATH)/Include"
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

	linkoptions {
		"-ld_classic"
	}

	files
	{
		"bin/ios/data"
	}

	if _OPTIONS["plist"] then
		files
		{
			_OPTIONS["plist"]
		}

		files
		{
			(pmtech_dir .. "/core/template/ios/Base.lproj/**.*"),
		}
	else
		files
		{
			(pmtech_dir .. "/core/template/ios/**.*"),
		}
	end

	excludes
	{
		("**.DS_Store")
	}

	xcodebuildresources
	{
		"bin/ios/data"
	}

	xcodebuildsettings
	{
		["IPHONEOS_DEPLOYMENT_TARGET"] = "14.0"
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

local function setup_web()
	file = io.open(("assets/file_lists/" .. s_project_name .. "_data.txt"), "r")
	if file then
		io.input(file)
		file_list = io.read()
		linkoptions { file_list }
		io.close(file)
	end

	filter "configurations:Debug"
		buildoptions {
			"-g4",
			"-s STACK_OVERFLOW_CHECK=1",
			"-s SAFE_HEAP=1",
			"-s DETERMINISTIC=1"
		}
		linkoptions {
			"-g4",
			"--source-map-base http://localhost:8000/web/",
			"-s STACK_OVERFLOW_CHECK=1",
			"-s SAFE_HEAP=1",
			"-s DETERMINISTIC=1"
		}

	filter {}
	targetextension (".html")
	links { "pen", "put" }
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
	elseif platform_dir == "web" then
		setup_web()
	end
end

local function setup_bullet()
	libdirs
	{
		(pmtech_dir .. "third_party/bullet/lib/" .. platform_dir)
	}

	filter "configurations:Debug"
		links { "bullet_monolithic_d" }

	filter "configurations:Release"
		links { "bullet_monolithic" }

	filter {}
end

local function setup_fmod()
	if platform == "web" then
		return
	end

	libdirs
	{
		(pmtech_dir .. "third_party/fmod/lib/" .. platform_dir)
	}
end

function setup_modules()
	setup_bullet()
	setup_fmod()
end

function create_dll(project_name, source_directory, root_directory)
	s_project_name = project_name
	project ( project_name )
		setup_product( project_name )
		kind ( binary_type )
		language "C++"

		print(pmtech_dir)
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

		location (root_directory .. "/build/" .. platform_dir)
		targetdir (root_directory .. "/bin/" .. platform_dir)
		debugdir (root_directory .. "/bin/" .. platform_dir)

		filter "configurations:Release"
			defines { "NDEBUG" }
			optimize "Speed"
			targetname (project_name)

		filter "configurations:Debug"
			defines { "DEBUG" }
			symbols "On"
			targetname (project_name .. "_d")
end

function create_binary(project_name, source_directory, root_directory, binary_type)
	s_project_name = project_name
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

		filter "configurations:Release"
			defines { "NDEBUG" }
			entrypoint "WinMainCRTStartup"
			optimize "Speed"
			targetname (project_name)
			libdirs
			{
				pmtech_dir .. "core/pen/lib/" .. platform_dir .. "/release",
				pmtech_dir .. "core/put/lib/" .. platform_dir .. "/release",
			}

		filter "configurations:Debug"
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

function setup_live_lib( project_name )
	if platform == "win32" then
		filter {}
		libdirs {
			"bin/win32"
		}
		dependson {
			"pmtech_editor"
		}

		filter "configurations:Debug"
		links {
			"pmtech_editor_d.lib"
		}
		filter "configurations:Release"
		links {
			"pmtech_editor.lib"
		}
		filter {}
	elseif platform == "osx" then
		filter {}
		linkoptions {
			"-undefined dynamic_lookup"
		}
	elseif platform == "linux" then
		filter {}
		linkoptions {
			"-fPIC",
			"-export-dynamic"
		}
	end

	project "pmtech_editor"
		filter "configurations:Debug"
			if platform == "osx" then
				defines {
					('LIVE_LIB="\\\"lib' .. project_name .. '_d.dylib\\\""')
				}
			elseif platform == "win32" then
				defines {
					('LIVE_LIB="\\\"' .. project_name .. '_d.dll\\\""')
				}
			elseif platform == "linux" then
				defines {
					('LIVE_LIB="\\\"lib' .. project_name .. '_d.so\\\""')
				}
			end
		filter "configurations:Release"
			if platform == "osx" then
				defines {
					('LIVE_LIB="\\\"lib' .. project_name .. '.dylib\\\""')
				}
			elseif platform == "win32" then
				defines {
					('LIVE_LIB="\\\"' .. project_name .. '.dll\\\""')
				}
			elseif platform == "linux" then
				defines {
					('LIVE_LIB="\\\"lib' .. project_name .. '.so\\\""')
				}
			end

	project ( project_name )
		setup_product( project_name )
		kind ( "SharedLib" )
		language "C++"

		if platform == "win32" then
			dependson {
				"pmtech_editor"
			}
		end
end

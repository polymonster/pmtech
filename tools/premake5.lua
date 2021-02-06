if _ACTION == "android-studio" then
	require "android_studio"
end

dofile "premake/options.lua"
dofile "premake/globals.lua"
dofile "premake/app_template.lua"

solution ("pmtech_tools_" .. platform)
	location ("build/" .. platform_dir) 
	configurations { "Debug", "Release" }
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	includedirs { "." }
		
dofile "../core/pen/project.lua"
dofile "../core/put/project.lua"

create_app_example("mesh_opt", script_path())
create_app_example("pmtech_editor", script_path())

-- win32 needs to export a lib for the live lib to link against
if platform == "win32" then
	configuration {"Debug"}
		prebuildcommands
		{
			"py -3 ../../../tools/pmbuild_ext/libdef.py ../../../core/put/lib/win32/debug/put.lib ../../../core/pen/lib/win32/debug/pen.lib -o pmtech_d.def",
		}
		linkoptions {
		  "/DEF:\"pmtech_d.def"
		}
	configuration {"Release"}
		prebuildcommands
		{
			"py -3 ../../../tools/pmbuild_ext/libdef.py ../../../core/put/lib/win32/release/put.lib ../../../core/pen/lib/win32/release/pen.lib -o pmtech.def"
		}
		linkoptions {
		  "/DEF:\"pmtech.def"
		}
end

-- dll to hot reload
create_dll("live_lib", "live_lib", script_path())

-- win32 needs to link against the export lib
if platform == "win32" then
	configuration {}
	dependson
	{
		"pmtech_editor"
	}
	libdirs
	{
		"bin/win32"
	}
	configuration {"Debug"}
		links
		{
			"pmtech_editor_d.lib"
		}
	configuration {"Release"}
		links
		{
			"pmtech_editor.lib"
		}
	configuration {}
elseif platform == "osx" then
	configuration {}
	linkoptions
	{
	    "-undefined dynamic_lookup"
	}
elseif platform == "linux" then
	configuration {}
	linkoptions
	{
	    "-fPIC",
		"-export-dynamic"
	}
end

project ( project_name )
	setup_product( "live_lib" )
	kind ( "SharedLib" )
	language "C++"
	
project "pmtech_editor"
	configuration { "Debug" }
	configuration { "Release" }
	
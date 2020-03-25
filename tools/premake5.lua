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
		
dofile "../core/pen/project.lua"
dofile "../core/put/project.lua"

create_app_example("mesh_opt", script_path())
create_app_example("pmtech_editor", script_path())

-- dll to hot reload
create_binary("live_lib", "live_lib", script_path(), "SharedLib" )
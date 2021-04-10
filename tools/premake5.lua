-- includes
if _ACTION == "android-studio" then
	require "android_studio"
end
dofile "premake/options.lua"
dofile "premake/globals.lua"
dofile "premake/app_template.lua"

-- solution
solution ("pmtech_tools_" .. platform)
	location ("build/" .. platform_dir) 
	configurations { "Debug", "Release" }
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	includedirs { "." }
		
-- core libs
dofile "../core/pen/project.lua"
dofile "../core/put/project.lua"

-- pmtech editor lib
dofile "pmtech.lua"

-- mesh optimiser
create_app_example("mesh_opt", script_path())

-- dll to hot reload
create_dll("live_lib", "live_lib", script_path())
setup_live_lib("live_lib")

	
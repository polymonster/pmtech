dofile "../tools/premake/globals.lua"
dofile "../tools/premake/app_template.lua"

-- Solution
solution "pen_examples"
	location "build"
	configurations { "Debug", "Release" }
	startproject "basic_triangle"
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	
-- Engine Project	
dofile "../pen/project.lua"

-- Toolkit Project	
-- dofile "../put/project.lua"

-- Example projects	
create_app( "empty_project", script_path() )
create_app( "basic_triangle", script_path() )

	
	
	

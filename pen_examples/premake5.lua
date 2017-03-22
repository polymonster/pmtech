--add extension options
newoption 
{
   trigger     = "renderer",
   value       = "API",
   description = "Choose a renderer",
   allowed = 
   {
      { "opengl", "OpenGL" },
      { "dx11",  "DirectX 11 (Windows only)" },
   }
}

newoption 
{
   trigger     = "xcode_target",
   value       = "TARGET",
   description = "Choose an xcode build target",
   allowed = 
   {
      { "osx", "OSX" },
      { "ios",  "iOS" },
   }
}

dofile "../tools/premake/globals.lua"
dofile "../tools/premake/app_template.lua"

-- Solution
solution "pen_examples"
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	startproject "basic_triangle"
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	
-- Engine Project	
dofile "../pen/project.lua"

-- Toolkit Project	
dofile "../put/project.lua"

-- Example projects	
-- ( project name, current script dir, link "put" )
create_app( "empty_project", script_path(), false )
create_app( "basic_triangle", script_path(), false )
create_app( "render_target", script_path(), true )
create_app( "texturing", script_path(), true )

	
	
	

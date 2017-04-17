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
solution "examples"
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	startproject "empty_project"
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	
-- Engine Project	
dofile "../pen/project.lua"

-- Toolkit Project	
dofile "../put/project.lua"

-- Example projects	
-- ( project name, current script dir, )
create_app( "empty_project", script_path() )
create_app( "basic_triangle", script_path() )
create_app( "textures", script_path() )
create_app( "render_target", script_path() )
create_app( "debug_text", script_path() )
create_app( "play_sound", script_path() )
create_app( "imgui", script_path() )
create_app( "input", script_path() )
create_app( "audio_player", script_path() )
	
	

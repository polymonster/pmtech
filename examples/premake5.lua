dofile "../tools/premake/options.lua"
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
create_app_example( "empty_project", script_path() )
create_app_example( "basic_triangle", script_path() )
create_app_example( "textures", script_path() )
create_app_example( "render_target", script_path() )
create_app_example( "debug_text", script_path() )
create_app_example( "play_sound", script_path() )
create_app_example( "imgui", script_path() )
create_app_example( "input", script_path() )
create_app_example( "audio_player", script_path() )
create_app_example( "shader_toy", script_path() )
create_app_example( "scene_editor", script_path() )
create_app_example( "rigid_body_primitives", script_path() )
	
	

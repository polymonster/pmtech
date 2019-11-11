if _ACTION == "android-studio" then
	require "android_studio"
end

dofile "../tools/premake/options.lua"
dofile "../tools/premake/globals.lua"
dofile "../tools/premake/app_template.lua"

-- Solution
solution ("pmtech_examples_" .. platform)
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	startproject "empty_project"
	buildoptions { build_cmd }
	linkoptions { link_cmd }
	includedirs
	{
	    "shader_structs"
	}
	
-- Engine Project	
dofile "../source/pen/project.lua"

-- Toolkit Project	
dofile "../source/put/project.lua"

-- Example projects	
-- ( project name, current script dir, )
create_app_example( "empty_project", script_path() )
create_app_example( "clear", script_path() )
create_app_example( "basic_triangle", script_path() )
create_app_example( "basic_texture", script_path() )
create_app_example( "render_target", script_path() )
create_app_example( "debug_text", script_path() )
create_app_example( "stencil_buffer", script_path() )
create_app_example( "rasterizer_state", script_path() )
create_app_example( "play_sound", script_path() )
create_app_example( "imgui", script_path() )
create_app_example( "input", script_path() )
create_app_example( "audio_player", script_path() )
create_app_example( "shader_toy", script_path() )
create_app_example( "pmtech_editor", script_path() )
create_app_example( "rigid_body_primitives", script_path() )
create_app_example( "physics_constraints", script_path() )
create_app_example( "instancing", script_path() )
create_app_example( "shadow_maps", script_path() )
create_app_example( "cubemap", script_path() )
create_app_example( "texture_formats", script_path() )
create_app_example( "skinning", script_path() )
create_app_example( "vertex_stream_out", script_path() )
create_app_example( "volume_texture", script_path() )
create_app_example( "multiple_render_targets", script_path() )
create_app_example( "maths_functions", script_path() )
create_app_example( "sdf_shadow", script_path() )
create_app_example( "post_processing", script_path() )
create_app_example( "sss", script_path() )
create_app_example( "pmfx_renderer", script_path() )
create_app_example( "dynamic_cubemap", script_path() )
create_app_example( "blend_modes", script_path() )
create_app_example( "entities", script_path() )
create_app_example( "complex_rigid_bodies", script_path() )
create_app_example( "basic_compute", script_path() )
create_app_example( "area_lights", script_path() )
create_app_example( "render_target_mip_maps", script_path() )
create_app_example( "ik", script_path() )
create_app_example( "stencil_shadows", script_path() )
create_app_example( "msaa_resolve", script_path() )
create_app_example( "compute_demo", script_path() )

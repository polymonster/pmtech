-- setup global variables to configure the projects
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
 
platform_dir = "win32"
build_cmd = ""
link_cmd = ""
renderer_dir = "dx11"

if _ACTION == "xcode4" then 
	platform_dir = "osx" 
	build_cmd = "-std=c++11 -stdlib=libc++"
	link_cmd = "-stdlib=libc++"
	renderer_dir = "opengl"
else
	if _OPTIONS["renderer"] then
   		renderer_dir = _OPTIONS["renderer"]
	end
end

function script_path()
   local str = debug.getinfo(2, "S").source:sub(2)
   return str:match("(.*/)")
end

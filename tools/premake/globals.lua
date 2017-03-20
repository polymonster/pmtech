-- setup global variables to configure the projects
platform_dir = "win32"
build_cmd = ""
link_cmd = ""
renderer_dir = "opengl"

if _OPTIONS["renderer"] then
	print( "rednerer" )
	renderer_dir = _OPTIONS["renderer"]
end

if _ACTION == "xcode4" then 
	
	platform_dir = "osx" 
	
	if _OPTIONS["xcode_target"] then
	print( "xcode_target" .. _OPTIONS["xcode_target"] )
   	platform_dir = _OPTIONS["xcode_target"]
end


	build_cmd = "-std=c++11 -stdlib=libc++ -sdkroot=iphoneos"
	link_cmd = "-stdlib=libc++"
	
	if not renderer_dir == "opengl" then
		print(	"renderer " .. renderer_dir .. " is not supported on platform " .. platform_dir .. " setting to opengl" )
		renderer_dir = "opengl"
	end
else

end

function script_path()
   local str = debug.getinfo(2, "S").source:sub(2)
   return str:match("(.*/)")
end

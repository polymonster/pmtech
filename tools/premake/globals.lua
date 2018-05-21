-- setup global variables to configure the projects
platform_dir = "win32"
build_cmd = ""
link_cmd = ""
renderer_dir = "opengl"
sdk_version = ""

if _OPTIONS["renderer"] then
	renderer_dir = _OPTIONS["renderer"]
end

if _OPTIONS["sdk_version"] then
	sdk_version = _OPTIONS["sdk_version"]
end

if _OPTIONS["platform_dir"] then
	platform_dir = _OPTIONS["platform_dir"]
end

if _OPTIONS["toolset"] then
	toolset(_OPTIONS["toolset"])
end

pmtech_dir = "../"
if _OPTIONS["pmtech_dir"] then
	pmtech_dir = _OPTIONS["pmtech_dir"]
end

if _ACTION == "gmake" then
	if platform_dir == "linux" then
		build_cmd = "-std=c++11"
	else
		build_cmd = "-std=c++11 -stdlib=libc++"
		link_cmd = "-stdlib=libc++"
	end
	
elseif _ACTION == "xcode4" then 
	
	platform_dir = "osx" 
	if _OPTIONS["xcode_target"] then
   	platform_dir = _OPTIONS["xcode_target"]
	end

	if platform_dir == "ios" then
		build_cmd = "-std=c++11 -stdlib=libc++"
		link_cmd = "-stdlib=libc++"
	else
		build_cmd = "-std=c++11 -stdlib=libc++"
		link_cmd = "-stdlib=libc++ -mmacosx-version-min=10.8"
	end
	
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

function windows_sdk_version()
	return "10.0.16299.0"
end

-- setup global variables to configure the projects
platform_dir = ""
platform = ""
build_cmd = ""
link_cmd = ""
renderer_dir = ""
sdk_version = ""
shared_libs_dir = ""
pmtech_dir = "../"

function setup_from_options()
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

    if _OPTIONS["pmtech_dir"] then
        pmtech_dir = _OPTIONS["pmtech_dir"]
    end
end

function script_path()
   local str = debug.getinfo(2, "S").source:sub(2)
   return str:match("(.*/)")
end

function windows_sdk_version()
	return "10.0.16299.0"
end

function setup_from_action()
    if _ACTION == "gmake" then
        if platform_dir == "linux" then
            build_cmd = "-std=c++11"
        else
            build_cmd = "-std=c++11 -stdlib=libc++"
            link_cmd = "-stdlib=libc++"
        end
    elseif _ACTION == "xcode4" then 
        platform_dir = "osx" 
        
        if not renderer_dir then
            renderer_dir = "opengl"
        end
        
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
		shared_libs_dir = ( pmtech_dir .. '../../third_party/shared_libs/' .. platform_dir .. '/' )
    elseif _ACTION == "android-studio" then 
        build_cmd = { "-std=c++11" }
    elseif _ACTION == "vs2017" then
        platform_dir = "win32" 
        build_cmd = "/Ob1" -- use force inline
		shared_libs_dir = (pmtech_dir .. '../../third_party/shared_libs/' .. platform_dir)
    end
    
    
    platform = platform_dir
end

function setup_env_ios()
	xcodebuildsettings
	{
		["ARCHS"] = "$(NATIVE_ARCH_ACTUAL)",
		["SDKROOT"] = "iphoneos11.4",
		["PRODUCT_BUNDLE_IDENTIFIER"] = "com.pmtech"
	}
end

function setup_env()
    if platform == "ios" then
        setup_env_ios()
    end
end

setup_from_options()
setup_from_action()
local function setup_win32()
    if renderer_dir == "vulkan" then
        includedirs
        {
            "$(VK_SDK_PATH)/Include"
        }
    elseif renderer_dir == "opengl" then         
        includedirs
        {
            "../../third_party/glew/include",
            "../../third_party/glew/src"
        }
    end
    systemversion(windows_sdk_version())
    disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
end

local function setup_ios()
    files 
    {  
        "source/posix/**.cpp",
        "source/mach/**.cpp"
    }
end

local function setup_osx()
    files 
    {  
        "source/posix/**.cpp",
        "source/mach/**.cpp"
    }
end

local function setup_linux()
    files 
    {  
        "source/posix/**.cpp"
    }
end

local function setup_web()
    files 
    {  
        "source/posix/**.cpp"
    }
end

local function setup_android()
    files 
    {  
        "source/posix/**.cpp"
    }
end

local function setup_platform()
    if platform_dir == "win32" then
        setup_win32()
    elseif platform_dir == "osx" then
        setup_osx()
    elseif platform_dir == "linux" then
        setup_linux()
    elseif platform_dir == "android" then
        setup_android()
    elseif platform_dir == "ios" then
        setup_ios()
    elseif platform_dir == "web" then
        setup_web()
    end
end

-- Project    
project "pen"
    setup_env()
    setup_platform_defines()
    setup_platform() 
    location ("build/" .. platform_dir)
    kind "StaticLib"
    language "C++"
    
    files 
    {
        "include/*.h",
        "source/*.cpp",
     
        "include/" .. platform_dir .. "/**.h", 

        "source/" .. platform_dir .. "/**.cpp",
        "source/" .. platform_dir .. "/**.mm",
    
        "../../third_party/str/*.cpp", 
    }
    
    includedirs 
    {
        "include",
        "include/" .. platform_dir, 
    
        "../../third_party", 
        "../../third_party/libstem_gamepad/source"
    }
    
    -- rendere selection, and allow for no renderer
    if string.len(renderer_dir) > 0 then
		files
		{   
			"include/" .. renderer_dir .. "/**.h",
			"source/" .. renderer_dir .. "/**.cpp",
			"source/" .. renderer_dir .. "/**.mm",
		}
		includedirs 
		{
			"include/" .. renderer_dir,
		}
    end
            
    configuration "Release"
        defines { "NDEBUG" }
        entrypoint "WinMainCRTStartup"
        optimize "Speed"
        targetdir ("lib/" .. platform_dir .. "/release")
        targetname "pen"
        
    configuration "Debug"
        defines { "DEBUG" }
        entrypoint "WinMainCRTStartup"
        symbols "On"
        targetdir ("lib/" .. platform_dir .. "/debug")
        targetname "pen"

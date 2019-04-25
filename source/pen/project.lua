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

local function setup_win32()
    systemversion(windows_sdk_version())
    disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
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
    end
end

-- Project    
project "pen"
    location ("build/" .. platform_dir)
    kind "StaticLib"
    language "C++"
    
    setup_env()
    setup_platform() 
    
    files 
    {
        "include/*.h",
        "source/*.cpp",
     
        "include/" .. platform_dir .. "/**.h", 
        "include/" .. renderer_dir .. "/**.h",
    
        "source/" .. renderer_dir .. "/**.cpp",
        "source/" .. renderer_dir .. "/**.mm",
    
        "source/" .. platform_dir .. "/**.cpp",
        "source/" .. platform_dir .. "/**.mm",
    
        "../../third_party/str/*.cpp", 
    }

    includedirs 
    {
        "include",

        "include/" .. platform_dir, 
        "include/" .. renderer_dir,
        
        "../../third_party", 
        "../../third_party/libstem_gamepad/source"
    }
            
    configuration "Release"
        defines { "NDEBUG" }
		entrypoint "WinMainCRTStartup"
		optimize "Speed"
        targetdir ("lib/" .. platform_dir .. "/release")
        targetname "pen"
        architecture "x64"
        
    configuration "Debug"
        defines { "DEBUG" }
		entrypoint "WinMainCRTStartup"
        symbols "On"
        targetdir ("lib/" .. platform_dir .. "/debug")
        targetname "pen"
        architecture "x64"

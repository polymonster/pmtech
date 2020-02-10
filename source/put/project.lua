bullet_lib_dir = "osx"
if platform_dir == "linux" then
    bullet_lib_dir = "linux"
end
if _ACTION == "vs2017" or _ACTION == "vs2015" then
    bullet_lib_dir = _ACTION
end
-- Project    
project "put"
    setup_env()
    setup_platform_defines()
    location ("build/" .. platform_dir)
    kind "StaticLib"
    language "C++"
    
    libdirs
    { 
        "../pen/lib/" .. platform_dir,
        "../../third_party/bullet/lib/" .. bullet_lib_dir,
    }
    
    -- need to refactor renderer defs to remove this
    if platform_dir == "win32" and renderer_dir == "opengl" then
        includedirs
        {
            "../../third_party/glew/include"
        }
    end
    
    includedirs
    {
        "source",
        
        "../pen/include/",
        "../pen/include/common", 
        "../pen/include/" .. platform_dir,
        "../pen/include/" .. renderer_dir,
          
        "../../third_party",
        "../../third_party/fmod/inc",
        "../../third_party/bullet/src/",
        "../../third_party/imgui",
        "../../third_party/sdf_gen",
        "../../third_party/meshoptimizer"
    }
    
    if _ACTION == "vs2017" or _ACTION == "vs2015" then
        systemversion(windows_sdk_version())
        disablewarnings { "4800", "4305", "4018", "4244", "4267", "4996" }
    end
                    
    files 
    { 
        "source/**.cpp",
        "source/**.h", 
        
        "../../third_party/imgui/*.cpp",
        "../../third_party/imgui/*.h",
        "../../third_party/sdf_gen/*.h",
        "../../third_party/sdf_gen/*.cpp",
        "../../third_party/bussik/*.h",
        "../../third_party/bussik/*.cpp",
        "../../third_party/meshoptimizer/*.cpp",
        "../../third_party/meshoptimizer/*.h",
        
        "../../third_party/maths/*.h"
    }
    includedirs { "include" }
    
    configuration "Debug"
        defines { "DEBUG" }
        entrypoint "WinMainCRTStartup"
        symbols "On"
        targetdir ("lib/" .. platform_dir .. "/debug")
        targetname "put"
        architecture "x64"
 
    configuration "Release"
        defines { "NDEBUG" }
        entrypoint "WinMainCRTStartup"
        optimize "Speed"
        targetdir ("lib/" .. platform_dir .. "/release")
        targetname "put"
        architecture "x64"

bullet_lib_dir = "osx"
if platform_dir == "linux" then
    bullet_lib_dir = "linux"
end
if _ACTION == "vs2017" or _ACTION == "vs2015" then
    bullet_lib_dir = _ACTION
end
-- Project    
project "put"
    location ("build/" .. platform_dir)
    kind "StaticLib"
    language "C++"
    
    if platform_dir == "ios" then defines { PEN_GLES3 } end
    
    libdirs
    { 
        "../pen/lib/" .. platform_dir,
        "../../third_party/bullet/lib/" .. bullet_lib_dir,
    }
    
    includedirs
    {
        "source",
        
        "../pen/include/",
        "../pen/include/common", 
        "../pen/include/" .. platform_dir,
        "../pen/include/" .. renderer_dir,
          
        "../../third_party/fmod/inc",
        "../../third_party/bullet/src/",
        "../../third_party/imgui",
        "../../third_party/sdf_gen",
        "../../third_party"
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
        "../../third_party/sdf_gen/*.cpp"
    }
    includedirs { "include" }
    
    configuration "Debug"
        defines { "DEBUG" }
        flags { "WinMain" }
        symbols "On"
        targetdir ("lib/" .. platform_dir .. "/debug")
        targetname "put"
        architecture "x64"
 
    configuration "Release"
        defines { "NDEBUG" }
        flags { "WinMain", "OptimizeSpeed" }
        links { "bullet_monolithic" }
        targetdir ("lib/" .. platform_dir .. "/release")
        targetname "put"
        architecture "x64"

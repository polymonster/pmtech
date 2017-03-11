-- Project	
project "put"
	location "build\\win32"
	kind "StaticLib"
	language "C++"
	
	libdirs
	{ 
		os.getenv("PEN_DIR") .. "lib\\win32",
		os.getenv("PEN_DIR") .. "\\third_party\\bullet\\lib",
	}
	
	includedirs
	{ 
		os.getenv("PEN_DIR") .. "include", 
		os.getenv("PEN_DIR") .. "include\\win32",  
		os.getenv("PEN_DIR") .. "\\third_party\\bullet\\include"
	}
		
	files { "include\\**.h", "source\\**.cpp" }
	includedirs { "include" }
		
	configuration "Debug"
		defines { "DEBUG" }
		flags { "WinMain", "Symbols" }
		targetdir "lib\\win32"
		targetname "put_d"
		links { "bullet_monolithic_do.lib" }
 
	configuration "Release"
		defines { "NDEBUG" }
		flags { "WinMain", "Optimize" }
		targetdir "lib\\win32"
		targetname "put"
		links { "bullet_monolithic.lib" }
-- Android Studio Premake Module

-- Module interface
	local m = {}

	newaction {
		trigger     = "android-studio",
		shortname   = "Android Studio",
		description = "Generate Android Studio Gradle Files",

		toolset  = "clang",

		-- The capabilities of this action

		valid_kinds     = { "ConsoleApp", "WindowedApp", "SharedLib", "StaticLib", "Makefile", "Utility", "None" },
		valid_languages = { "C", "C++" },
		valid_tools     = {
			cc = { "clang" },
		},
	}
	
	function workspace(wks)
		print("oh hai workspace")	
	end
	
	function project(base, prj)
		print(base)
		print("oh hai project")	
	end
	
	print("The android studio module has loaded!")

-- Return module interface
	return m

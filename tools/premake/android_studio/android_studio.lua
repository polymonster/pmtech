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
		
		onStart = function()
			print("Starting android studio generation")
		end,

		onWorkspace = function(wks)
			printf("Generating android studio workspace '%s'", wks.name)
		end,

		onProject = function(prj)
			printf("Generating android studio project '%s'", prj.name)
		end,

		execute = function()
			print("Executing android studio action")
		end,

		onEnd = function()
			print("Android studio generation complete")
		end
	}
	
	function m.workspace(wks)
		print("oh hai workspace")	
	end
	
	function m.project(base, prj)
		print(base)
		print("oh hai project")	
	end
	
	print("Premake: loaded module android-studio")

-- Return module interface
	return m

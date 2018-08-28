-- Android Studio Premake Module

-- Module interface
	local m = {}
	
	local p = premake
	local project = p.project
	local workspace = p.workspace
	local config = p.config
	local fileconfig = p.fileconfig
	local tree = p.tree

	newaction {
		trigger     = "android-studio",
		shortname   = "Android Studio",
		description = "Generate Android Studio Gradle Files",

		toolset  = "clang",

		-- The capabilities of this action

		valid_kinds = { 
			"ConsoleApp", 
			"WindowedApp", 
			"SharedLib", 
			"StaticLib", 
			"Makefile", 
			"Utility", 
			"None" 
		},
		valid_languages = { "C", "C++" },
		valid_tools     = {
			cc = { "clang" },
		},
				
		-- function overloads
		onStart = function()
			print("Starting android studio generation")
		end,

		onWorkspace = function(wks)
			p.generate(wks, ".gradle", m.generate_workspace)
			p.generate(wks, "settings.gradle", m.generate_workspace_settings)
		end,

		onProject = function(prj)
			p.generate(prj, prj.name .. "/" .. prj.name .. ".gradle", m.generate_project)
		end,

		execute = function()
			print("Executing android studio action")
		end,

		onEnd = function()
			print("Android studio generation complete")
		end
	}
	
-- Functions
	function m.generate_workspace(wks)
		p.x('// workspace %s', wks.name)
		p.push('buildscript {')
		p.push('repositories {')
		p.w('jcenter()')
		p.pop('}')
		p.push('dependencies {')
		p.w('com.android.tools.build:gradle:2.3.2')    
		p.pop('}')
		p.pop('}')
		
		p.push('allprojects {')
		p.push('repositories {')
		p.w('jcenter()')
		p.pop('}')
		p.pop('}')
	end
	
	function m.generate_workspace_settings(wks)
		for prj in workspace.eachproject(wks) do
			p.x('include ":%s"', prj.name)
			p.x('project(":%s").projectDir = file("%s")', prj.name, prj.location)
		end
	end
	
	function get_android_program_kind(premake_kind)
		if premake_kind == "WindowedApp" then 
			return "com.android.model.application"
		elseif premake_kind == "ConsoleApp" then
			return "com.android.model.application"
		elseif premake_kind == "StaticLib" then
			return "com.android.model.library"
		end
	end
	
	function m.generate_project(prj)
		p.x("apply plugin: '%s'", get_android_program_kind(prj.kind))
	
		p.push('model {')
		p.push('android {')
		
		-- todo: set sdk values from premake
		p.w('compileSdkVersion = 25')
        p.w('buildToolsVersion = "25.0.3"')
        
        p.push('defaultConfig.with {')
        p.w('minSdkVersion.apiLevel = 19')
        p.w('targetSdkVersion.apiLevel = 25')
        p.w('versionCode = 1')
        p.w('versionName = "1.0"')
        
        p.pop('}') -- defaultConfig.with 
        p.pop('}') -- android
        
        p.push('android.ndk {')
        p.x('moduleName = "%s"', prj.name)
        p.w('stl = "gnustl_static"')
        p.w('platformVersion = "25"')
        
        -- todo cpp flags        
		-- cppFlags.addAll(["-std=c++11", "-Wall", "-frtti", "-fexceptions", "-nostdlib", "-fno-short-enums", "-ffunction-sections", "-fdata-sections"])
		
        p.pop('}') -- android.ndk
        
		p.pop('}') -- model
		
		--[[
		for key,value in pairs(prj) do
   	 		print("found member " .. key);
		end
		
		p.push('project = {')
		p.x('name = "%s",', prj.name)
		p.w('uuid = "%s",', prj.uuid)
		p.w('kind = "%s"', prj.kind)
		p.pop('}')
		
		for cfg in project.eachconfig(prj) do
			print(cfg.name)
			for _, incdir in ipairs(cfg.includedirs) do
				print(incdir)
				testname = path.join(incdir, pch)
				if os.isfile(testname) then
					pch = project.getrelative(cfg.project, testname)
					-- p.w('Include Dirs = "%s"', node.name)
					break
				end
			end
			for _, libdir in ipairs(cfg.libdirs) do
			print(libdir)
			end
			for _, file in ipairs(cfg.files) do
			print(file)
			end
			for _, define in ipairs(cfg.defines) do
			print(define)
			end
			for _, cppflag in ipairs(cfg.buildoptions) do
			print(cppflag)
			end
			for _, ldflag in ipairs(cfg.linkoptions) do
			print(ldflag)
			end
		end
		
		local tr = project.getsourcetree(prj, nil , false)
		tr.project = prj
		
		-- todo configs.. (debug / release etc)
		
		-- src tree / filters ?? or use raw file list above
		tree.traverse(tr, {
			onnode = function(node)
				p.w('File = "%s"', node.name)
			end
		})
		
		-- frameworks
		tr.frameworks = tree.new("Frameworks")
		for cfg in project.eachconfig(prj) do
			for _, link in ipairs(config.getlinks(cfg, "system", "fullpath")) do
				p.w('Link = "%s"', link)
			end
		end
		
		-- projects / dependencies
		tr.projects = tree.new("Projects")
		for _, dep in ipairs(project.getdependencies(prj, "linkOnly")) do
			p.w('Dep = "%s"', dep.name)
		end
		for _, dep in ipairs(project.getdependencies(prj, "dependOnly")) do
			p.w('Dep = "%s"', dep.name)
		end
		
		-- todo
		-- cpp flags
		-- "-I" include dirs
		-- ld flags
		-- "-L" lib dirs
		-- ld libs
		
		-- todo
		-- add to premake "links" 
		-- "atomic", "android", "OpenSLES", 
		-- "z", "GLESv1_CM", "GLESv2", "GLESv3", "log"
		--]]
	end
	
	print("Premake: loaded module android-studio")

-- Return module interface
	return m

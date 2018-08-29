-- Android Studio Premake Module

-- Module interface
	local m = {}
	
	local p = premake
	local project = p.project
	local workspace = p.workspace
	local config = p.config
	local fileconfig = p.fileconfig
	local tree = p.tree
	local src_dirs = {}

	newaction {
		trigger     = "android-studio",
		shortname   = "Android Studio",
		description = "Generate Android Studio Gradle Files",

		toolset  	= "clang",

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
		valid_tools = {
			cc = { "clang" },
		},
				
		-- function overloads
		onStart = function()
			print("Starting android studio generation")
		end,

		onWorkspace = function(wks)
			p.generate(wks, "build.gradle", m.generate_workspace)
			p.generate(wks, "settings.gradle", m.generate_workspace_settings)
		end,

		onProject = function(prj)
			p.generate(prj, prj.name .. "/build.gradle", m.generate_project)
			p.generate(prj, prj.name .. "/src/main/AndroidManifest.xml", m.generate_manifest)
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
		p.w("classpath 'com.android.tools.build:gradle-experimental:0.7.3'")  
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
			p.x('project(":%s").projectDir = file("%s/%s")', prj.name, prj.location, prj.name)
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
	
	function get_dir(file)
		return string.match(file, ".*/")
	end
	
	function m.generate_manifest(prj)
		-- auto generate stub android manifest for libraries
		p.w('<?xml version="1.0" encoding="utf-8"?>')
		p.push('<manifest xmlns:android="http://schemas.android.com/apk/res/android"')
		p.x('package="lib.%s"', prj.name)
		p.w('android:versionCode="1"')
		p.w('android:versionName="1.0" >')
		p.w('<uses-sdk android:minSdkVersion="19" />')
		
		-- if no manifest is specified for the app project:
		-- autogenerate one so at least we can try to compile and link
		if prj.kind == "WindowedApp" then
			p.push('<application')
			p.w('android:allowBackup="true"')
			p.x('android:label="%s" />', prj.name)
			p.push('<activity')
			p.w('android:name="com.main.activity"')
			p.w('android:label="main_activity"')
			p.w('android:configChanges="orientation|screenSize">')
			p.push('<intent-filter>')
			p.w('<action android:name="android.intent.action.MAIN" />')
			p.w('<category android:name="android.intent.category.LAUNCHER" />')
			p.pop('</intent-filter>')
			p.pop('</activity>')
		else
			p.pop('<application/>')
		end
		p.pop('</manifest>')
	end
	
	function m.generate_project(prj)
		p.x("apply plugin: '%s'", get_android_program_kind(prj.kind))
	
		p.push('model {')
		p.push('android {')
		
		-- todo: pass these values from premake.lua
		p.w('compileSdkVersion = 25')
        p.w('buildToolsVersion = "25.0.3"')
        
        p.push('defaultConfig.with {')
        p.w('minSdkVersion.apiLevel = 19')
        p.w('targetSdkVersion.apiLevel = 25')
        p.w('versionCode = 1')
        p.w('versionName = "1.0"')
        
        p.pop('}') -- defaultConfig.with 
        
        p.push('ndk {')
        p.x('moduleName = "%s"', prj.name)
        p.w('stl = "gnustl_static"')
        p.w('platformVersion = "19"')

        p.pop('}') -- ndk
        
        p.push('buildTypes {')
        for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
								
			p.push('ndk {')
			
        	-- cpp flags
        	for _, cppflag in ipairs(cfg.buildoptions) do
        		p.x('cppFlags.add("%s")', cppflag)
			end
			
			-- include directories
			for _, incdir in ipairs(cfg.includedirs) do
				p.x('cppFlags.add("-I%s")', incdir)	
			end
			
			-- ld flags
			for _, ldflag in ipairs(cfg.linkoptions) do
				p.x('ldFlags.add("%s")', ldflag)
			end
			
			-- lib directories
			for _, libdir in ipairs(cfg.libdirs) do
				p.x('ldFlags.add("-L%s")', libdir)
			end
			
			-- defines
			for _, define in ipairs(cfg.defines) do
				p.x('ldFlags.add("-D%s")', define)
			end
			
			-- links
			for _, link in ipairs(config.getlinks(cfg, "system", "fullpath")) do
				p.x('ldLibs.add("%s")', link)
			end
						
			p.pop('}') -- ndk
        	p.pop('}') -- cfg.name
		end
		
        p.pop('}') -- buildType
				
		-- source files from premake
		p.push('sources {')
		for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
			
			-- get srcDirs because gradle experimental with jni does not support adding single files :(
			dir_list = nil
			for _, file in ipairs(cfg.files) do
				if os.isfile(file) then
					new_dir = get_dir(file)
					if dir_list == nil then dir_list = ""
					else dir_list = (dir_list .. ', ') 
					end
					dir_list = (dir_list .. '"' .. new_dir .. '"')
				end
			end
			
			-- c/cpp/h files
			p.push('jni {')
			p.push('source {')
			p.x('srcDirs = [%s]', dir_list)
			p.pop('}') -- source
			p.pop('}') -- jni
						
			-- java files
			p.push('java {')
			p.push('source {')
			for _, file in ipairs(cfg.files) do
				if os.isfile(file) then
					if string.find(file, ".java") then
						p.x('includes.add("%s")', file)
					end
				end
			end
			p.pop('}') -- source
			p.pop('}') -- java
			
			-- res files
			p.push('res {')
			p.push('source {')
			p.w("srcDirs = ['res']")
			p.pop('}') -- source
			p.pop('}') -- res
			
			-- manifest files
			p.push('manifest {')
			p.push('source {')
			p.w("srcDirs = ['.']")
			p.pop('}') -- source
			p.pop('}') -- manifest
					
			p.pop('}') -- cfg.name
		end
		p.pop('}') -- sources				
        p.pop('}') -- android
		
		p.push('android.lintOptions {')
		p.w("abortOnError = false")
		p.pop('}')
		
		p.pop('}') -- model
		
		p.push('dependencies {')
		-- todo: pass these from premake.lua
		p.w("compile 'com.android.support:support-v4:23.0.0'")
		for _, dep in ipairs(project.getdependencies(prj, "dependOnly")) do
			p.x("compile project(':%s')", dep.name)
		end
		for _, dep in ipairs(project.getdependencies(prj, "linkOnly")) do
			p.x("compile project(':%s')", dep.name)
		end
		p.pop('}')
	end
	
	print("Premake: loaded module android-studio")

-- Return module interface
	return m

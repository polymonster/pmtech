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
		valid_languages = { "C", "C++", "Java" },
		valid_tools = {
			cc = { "clang" },
		},
				
		-- function overloads
		onWorkspace = function(wks)
			p.generate(wks, "settings.gradle", m.generate_workspace_settings)
			p.generate(wks, "build.gradle", m.generate_workspace)
		end,

		onProject = function(prj)
			p.generate(prj, prj.name .. "/src/main/AndroidManifest.xml", m.generate_manifest)
			p.generate(prj, prj.name .. "/build.gradle", m.generate_project)
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
		-- look for a manifest in project files
		for cfg in project.eachconfig(prj) do		
			for _, file in ipairs(cfg.files) do
				if string.find(file, "AndroidManifest.xml") then
					-- copy contents of manifest and write with premake
					manifest = io.open(file, "r")
					xml = manifest:read("*a")
					manifest:close()
					p.w(xml)
					return
				end
			end
		end
	
		-- auto generate stub android manifest
		p.w('<?xml version="1.0" encoding="utf-8"?>')
		p.push('<manifest xmlns:android="http://schemas.android.com/apk/res/android"')
		p.x('package="lib.%s"', prj.name)
		p.w('android:versionCode="1"')
		p.w('android:versionName="1.0" >')
		p.w('<uses-sdk android:minSdkVersion="19" />')
		p.pop('<application/>')
		p.pop('</manifest>')
	end
		
	function m.add_sources(cfg, category, exts, excludes, strip)		
		-- get srcDirs because gradle experimental with jni does not support adding single files :(
		local dir_list = nil
		for _, file in ipairs(cfg.files) do
			skip = false
			for _, exclude in ipairs(excludes) do
				if string.find(file, exclude) then
					skip = true
					break
				end
			end
			if not skip then
				for _, ext in ipairs(exts) do
					file_ext = path.getextension(file)
					if file_ext == ext then
						if (dir_list == nil) then dir_list = ""
						else dir_list = (dir_list .. ', ') 
						end
						new_dir = get_dir(file)
						if strip then
							loc = string.find(new_dir, strip)
							if (loc) then
								new_dir = new_dir:sub(0, loc-1 + string.len(strip))
							end
						end
						dir_list = (dir_list .. '"' .. new_dir .. '"')
					end
				end
			end
		end
				
		if dir_list then 
			p.push((category .. ' {'))
			p.push('source {')
			p.x('srcDirs = [%s]', dir_list)
			p.pop('}') -- source
			p.pop('}') -- category
		end
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
        
        sys_root = os.getenv("NDK_SYS_ROOT")
		if sys_root == nil then
			sys_root = ""
			print("Error: Missing NDK_SYS_ROOT environment var")
		end
				
        p.push('ndk {')
		p.x('moduleName = "%s"', prj.name)
        p.w('stl = "gnustl_static"')
        p.w('platformVersion = "25"')
        p.w('abiFilters.add("armeabi-v7a")')
		p.x('cppFlags.add("-I%s/usr/include")', sys_root)
		p.x('cppFlags.add("-I%s/usr/include/arm-linux-androideabi")', sys_root)

        p.pop('}') -- ndk
        
        p.push('buildTypes {')
        for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
								
			p.push('ndk {')
			-- p.x('moduleName = "%s"', cfg.targetname)
					
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
				if not path.getextension(link) == ".aar" then
					p.x('ldLibs.add("%s")', link)
				end
			end
						
			p.pop('}') -- ndk
        	p.pop('}') -- cfg.name
		end
		
        p.pop('}') -- buildType
				
		-- source files from premake
		jni_sources = {}
		java_sources = {}
		resources = {}
		
		p.push('sources {')
		for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
									
			m.add_sources(cfg, 'jni', {".cpp", ".c", ".h", ".hpp"}, {}) 
			m.add_sources(cfg, 'java', {'.java'}, {})
			m.add_sources(cfg, 'res', {'.png', '.xml'}, {"AndroidManifest.xml"}, "/res/")
					
			p.pop('}') -- cfg.name
		end
		p.pop('}') -- sources				
        p.pop('}') -- android
		
		-- lint options to avoid abort on error
		p.push('android.lintOptions {')
		p.w("abortOnError = false")
		p.pop('}')
		
		-- flavours
		--[[
		p.push('android.productFlavors {')
		p.push('create("x86") {')
		p.push('ndk {')
		p.w('abiFilters.add("x86")')
		p.x('cppFlags.add("%s/usr/include")', sys_root)
		p.x('cppFlags.add("%s/usr/include/i686-linux-android")', sys_root)
		p.pop('}')
		p.pop('}')
		p.push('create("armeabi-v7a") {')
		p.push('ndk {')
		p.w('abiFilters.add("armeabi-v7a")')
		p.x('cppFlags.add("-I%s/usr/include")', sys_root)
		p.x('cppFlags.add("-I%s/usr/include/arm-linux-androideabi")', sys_root)
		p.pop('}')
		p.pop('}')
		p.pop('}')
		--]]
		
		p.pop('}') -- model
		
		p.push('dependencies {')
		-- aar link.. jar too?
		for cfg in project.eachconfig(prj) do
			for _, link in ipairs(config.getlinks(cfg, "system", "fullpath")) do
				ext = path.getextension(link)
				if ext == ".aar" then
					p.x("compile (name:'%s', ext:'%s')", path.getbasename(link), ext:sub(2, 4))
				end
			end
		end
		
		p.w("compile 'com.android.support:support-v4:23.0.0'")
		for _, dep in ipairs(project.getdependencies(prj, "dependOnly")) do
			p.x("compile project(':%s')", dep.name)
		end
		
		p.pop('}')
	end
	
	print("Premake: loaded module android-studio")

-- Return module interface
	return m
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

-- Premake extensions
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
	
	p.api.register 
	{
		name = "gradleversion",
		scope = "workspace",
		kind = "string"
	}
	
	p.api.register 
	{
		name = "androiddependencies",
		scope = "config",
		kind = "list:string"
	}
	
	p.api.register 
	{
		name = "androidsdkversion",
		scope = "config",
		kind = "string"
	}
	
	p.api.register 
	{
		name = "androidbuildtoolsversion",
		scope = "config",
		kind = "string"
	}
	
	p.api.register 
	{
		name = "androidminsdkversion",
		scope = "config",
		kind = "string"
	}
		
-- Functions
	function m.generate_workspace(wks)
		p.x('// workspace %s', wks.name)
		p.push('buildscript {')
		p.push('repositories {')
		p.w('jcenter()')
		p.w('google()')
		p.pop('}') -- repositories
		p.push('dependencies {')
		
		if wks.gradleversion then
			p.x("classpath '%s'", wks.gradleversion)
		else
			p.w("classpath 'com.android.tools.build:gradle-experimental:0.7.3'")
		end  
		
		p.pop('}') -- dependencies
		p.pop('}') -- build scripts
		
		p.push('allprojects {')
		p.push('repositories {')
		p.w('jcenter()')
		p.w('google()')
		
		-- add lib dirs from linking .aar or .jar files
		dir_list = nil
		for prj in workspace.eachproject(wks) do
			for cfg in project.eachconfig(prj) do
				for _, libdir in ipairs(cfg.libdirs) do
					if dir_list == nil then
						dir_list = ""
					else
						dir_list = (dir_list .. ', ')
					end
					dir_list = (dir_list .. '"' .. libdir .. '"')
				end
			end
		end
		
		if dir_list then
			p.push('flatDir {')
			p.x('dirs %s', dir_list)
			p.pop('}') -- flat dir
		end
		
		p.pop('}') -- repositories
		p.pop('}') -- all projects
	end
	
	function m.generate_workspace_settings(wks)
		for prj in workspace.eachproject(wks) do
			p.x('include ":%s"', prj.name)
			p.x('project(":%s").projectDir = file("%s/%s")', prj.name, prj.location, prj.name)
		end
	end
	
	function get_android_program_kind(premake_kind)
		local premake_to_android_kind =
		{
			["WindowedApp"] = "com.android.application",
			["ConsoleApp"] = "com.android.application",
			["StaticLib"] = "com.android.library",
			["SharedLib"] = "com.android.library",
		}
		return premake_to_android_kind[premake_kind]
	end
		
	function get_cmake_program_kind(premake_kind)
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
			p.x((category .. '.srcDirs += [%s]'), dir_list)
		end
	end
	
	function generate_gradle_cpp()
		p.x("apply plugin: '%s'", get_android_program_kind(prj.kind))
	
		p.push('model {')
		p.push('android {')
				
		-- sdk / ndk etc
		for cfg in project.eachconfig(prj) do
			if cfg.androidsdkversion == nil then
				cfg.androidsdkversion = "25"
			end
			if cfg.androidbuildtoolsversion == nil then
				cfg.androidbuildtoolsversion = "25.0.3"
			end
			if cfg.androidminapilevel == nil then
				cfg.androidminapilevel = "19"
			end			
			p.x('compileSdkVersion = %s', cfg.androidsdkversion)
			p.x('buildToolsVersion = "%s"', cfg.androidbuildtoolsversion)
			p.push('defaultConfig.with {')
			p.x('minSdkVersion.apiLevel = %s', cfg.androidminapilevel)
			p.x('targetSdkVersion.apiLevel = %s', cfg.androidsdkversion)
			p.w('versionCode = 1')
			p.w('versionName = "1.0"')
			p.pop('}') -- defaultConfig.with 
			break
		end

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
        
        
		-- gradle config for experimental 0.7.3 with c++ support
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
			break
		end
		
		-- android dependencies
		for cfg in project.eachconfig(prj) do
			if cfg.androiddependencies then
				for _, dep in ipairs(cfg.androiddependencies) do
					p.x("compile '%s'", dep)
				end
			end
			break
		end
		
		-- project compile links
		for _, dep in ipairs(project.getdependencies(prj, "dependOnly")) do
			p.x("compile project(':%s')", dep.name)
		end
		
		p.pop('}')
	end
	
	function generate_cmake(prj)
		-- todo
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
	end
	
	function m.generate_project(prj)
		-- gradle config for gradle 4+
		print(get_android_program_kind(prj.kind))
		p.x("apply plugin: '%s'", get_android_program_kind(prj.kind))
		
		p.push('android {')
		
		-- sdk / ndk etc
		for cfg in project.eachconfig(prj) do
			-- set defaults
			if cfg.androidsdkversion == nil then
				cfg.androidsdkversion = "25"
			end
			if cfg.androidminsdkversion == nil then
				cfg.androidminsdkversion = "19"
			end		
			p.x('compileSdkVersion %s', cfg.androidsdkversion)
			p.push('defaultConfig {')
			p.x('minSdkVersion %s', cfg.androidminsdkversion)
			p.x('targetSdkVersion %s', cfg.androidsdkversion)
			p.w('versionCode 1')
			p.w('versionName "1.0"')
			p.pop('}') -- defaultConfig.with 
			break
		end
				
		p.push('buildTypes {')
        for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
        	p.pop('}') -- cfg.name
        end
        p.pop('}') -- build types
        	
		-- cmake
		p.push('externalNativeBuild {')
		p.push('cmake {')
		p.w('path "CMakeLists.txt"')
		p.pop('}') -- cmake
		p.pop('}') -- externalNativeBuild
		
		-- java and resource files
		p.push('sourceSets {')
		for cfg in project.eachconfig(prj) do
        	p.push(string.lower(cfg.name) .. ' {')
			m.add_sources(cfg, 'java', {'.java'}, {})
			m.add_sources(cfg, 'res', {'.png', '.xml'}, {"AndroidManifest.xml"}, "/res/")
			p.pop('}') -- cfg.name
		end
		p.pop('}') -- sources
		
		-- lint options to avoid abort on error
		p.push('lintOptions {')
		p.w("abortOnError = false")
		p.pop('}')
		
		p.pop('}') -- android
				
		-- project dependencies, java links, etc
		p.push('dependencies {')
		
		-- aar link.. jar too?
		for cfg in project.eachconfig(prj) do
			for _, link in ipairs(config.getlinks(cfg, "system", "fullpath")) do
				ext = path.getextension(link)
				if ext == ".aar" then
					p.x("compile (name:'%s', ext:'%s')", path.getbasename(link), ext:sub(2, 4))
				end
			end
			break
		end
		
		-- android dependencies
		for cfg in project.eachconfig(prj) do
			if cfg.androiddependencies then
				for _, dep in ipairs(cfg.androiddependencies) do
					p.x("compile '%s'", dep)
				end
			end
			break
		end
		
		-- project compile links
		for _, dep in ipairs(project.getdependencies(prj, "dependOnly")) do
			p.x("compile project(':%s')", dep.name)
		end
		
		p.pop('}') -- dependencies
	end
	
	print("Premake: loaded module android-studio")

-- Return module interface
	return m
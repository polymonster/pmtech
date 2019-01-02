--add extension options
newoption 
{
   trigger     = "renderer",
   value       = "API",
   description = "Choose a renderer",
   allowed = 
   {
      { "opengl", "OpenGL (macOS, linux, iOS, Android)" },
      { "dx11",  "DirectX 11 (Windows only)" },
      { "metal", "Metal (macOS, iOS only)" }
   }
}

newoption 
{
   trigger     = "sdk_version",
   value       = "version",
   description = "Specify operating system SDK",
}

newoption 
{
   trigger     = "platform_dir",
   value       = "dir",
   description = "specify platform specifc src folder",
}

newoption 
{
   trigger     = "toolset",
   value       = "value",
   description = "specify toolset (clang, gcc)",
}

newoption 
{
   trigger     = "xcode_target",
   value       = "TARGET",
   description = "Choose an xcode build target",
   allowed = 
   {
      { "osx", "OSX" },
      { "ios",  "iOS" },
   }
}

newoption 
{
   trigger     = "pmtech_dir",
   value       = "dir",
   description = "specify location of pmtech in relation to project"
}

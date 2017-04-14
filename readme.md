# Welcome to pmtech!

This work is still very much a work in progress, The win32 platform with dx11 is mostly complete, I have been using this for a game project. I am currently porting to OSX and iOS, both platforms are up and running and the opengl renderer (gles3 and gl3) is now in progress.. more platforms and renderers like metal, vulkan, linux or android will hopefully be added in future.

**pen** *pmtech engine* 

This project contains platform / operating system specific code, it provides abstractions for:
- Renderer (dx11, opengl)
- Window / Entry (win32, ox, ios)
- Input / OS Message Pump (win32, osx)
- Audio (fmod)
- Memory (win32, posix)
- String (for portability with windows unicode)
- File System (win32, dirent / stdlib)
- Threads / Syncronisation Primitives (win32, posix)
- Timers (win32, posix)
- Job Threads / Syncronisation

The renderer runs on its own thread, with all user submitted commands from the game thread being stored in a command buffer for dispatch later on a dedicated thread or core, this paralellises all graphics api driver overhead, the audio api and physics api (see put) also run the same way. I still need to work on giving greater control to which cores these tasks get run on and I have added provision for users to skip the auto generation of threads so they can replace it with their own job management system.

**put** *pmtech utility toolkit*

This project contains code that will be re-used across different projects but contains no platform specific code, it contains:
- "Loader" - Contains functions to load (and hot load) textures (DDS), models, skeletons, animations and shaders
- Bullet Physics - With a deferred command buffer API like the pen renderer and audio system, allowing physics simulate to run independantly on it's own thread.
- Debug Renderer - Helpers for drawing 3d and 2d lines, boxes, text, and other debug primitives.
- Scalar (float) Maths library - Vector, Matrix, Quaternion, Intersection tests and functions
- Thirdparty stuff
- ImGui - super cool and fast ui rendering for development.
- JSON for modern c++ - great for working with data that has been generated with python.

**tools**

Tool scripts written in python and using some c++ executables to build data:
- Collada to Binary - Models, skeletons and animations.
- Textures - Compression and conversion using NVTT (Nvidia).
- Premake5 - All projects are configured using premake and are IDE agnostic.
- ios project genetion - Simple script to copy ios files and fixup xcode to support ios projects.
- Shader Compiler - FXC offline shader compilation.
- Generic Shader Language - hlsl sm4 like shaders can be written, with a single file defining ps and vs main functions, a python script and some macros help porting between glsl and hlsl. A JSON metadata file is generated along with each shader program to provide reflection information to help generate d3d input layouts, gl vertex arrays and bind gl uniforms, uniform buffers and textures to the correct locations.

**examples**

This solution / workspace contains multiple examples of how to use the API's and set up projects, I have been using them to aid porting, starting with a simple windowed application using minimal dependencies, samples introduce more dependancies as they go along, this would also be an ideal place to add some unit tests and continously test functionality of the engine.

- empty_project - First port of call to get a platform compiling, it creates an empty window with no rendering context.
- basic_traingle - introduces a rendering context, clear sceen, shader loading / binding, vertex buffer and non-indexed draw calls.
- debug_font - introduces shader loading with the "put" library, vertex buffer updating and debug text rendering.
- textures - Introduces texture loading using the "put" library, index buffers, indexed draw calls, texture samplers and texture binding.
- render_target - Introduces render target creation / binding, shader program loading using the "put" library to automatically generate input layouts from the shaders input signature.
- play_sound - Introduces linking to fmod 5 and some basic audio functions.
- Imgui - Displays the Imgui test example, this also introduces a few new rendering festures that are required, blending, depth stencil states and scissor testing.

*Getting started*

To start a new project make sure it is located in pmtech/<project_dir>/ this is important because all paths to tools and other libraries are relative to this location. The examples solution is set up illustrating this layout.. all paths are relative to avoid having to deal with setting environment variables to locate various parts of the project.

An application just needs to define and initialise the pen::window_creation_params pen_window struct, and will be given a main loop, which can be defined in a separate project, there are helper functions to easily create new projects in pen/premake_app.lua and the pen_examples/premake5.lua has examples of how to set up projects. 

Use the make_projects batch file on windows or shell script on osx to generate IDE solutions or workspaces, edit these files to change configuration settings or to see how the current ones work.

The make projects scripts also contain command lines to build shaders and textures for the relevant platform.

supply --help to premake5 for more options on project configuration from inside the .lua files.

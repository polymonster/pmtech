# Welcome to pmtech!

A lightweight code base with powerful features that can be used for games, 3d and real-time applications. It offers cross platform support for osx, win32 and ios with opengl 3 and directx 11 renderers. 

Core systems such as rendering, audio and physics are handled asyncronously on consumer threads which process command buffers that are created on the user thread, the command buffer api's provide thread safe access to add commands or read data back from a consumer thread.

**pen** *pmtech engine* 

This project contains platform / operating system specific code, it provides abstractions for:
- Renderer (dx11, opengl)
- Window / Input / OS Message Pump (win32, osx, ios)
- Audio (fmod)
- Memory (win32, posix)
- File System (win32, dirent / stdlib)
- Threads / Syncronisation Primitives (win32, posix)
- Timers (win32, mach)
- Job Threads / Syncronisation

**put** *pmtech utility toolkit*

This project contains code that will be re-used across different projects but contains no platform specific code, it contains:
- Loader - Load textures (DDS), models, skeletons, animations and shaders, as well as supporting hot loading assets.
- Debug Renderer - Helpers for drawing 3d and 2d lines, boxes, text, and other primitives.
- Scalar (float) Maths library - Vector, Matrix, Quaternion, Intersection tests and functions.
- Physics command buffer api.
- ImGui file browser and other ui utilities.

** thirdparty **
- ImGui - super cool and fast ui rendering for development.
- JSON for modern c++ - great for working with data that has been generated with python.
- Bullet Physics - Wrapped up in a command buffer api for asyncronous update and thread safe interactions.

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
- Audio Player - Introduces more audio features and supplies a UI to play and control audio files, it also introduces fft and some beat-detection code along with graphs and visualisations of the fft this is r&d work in progress.
- Shader Toy - Introduces shader hot loading and a test bed for binding textures, samplers and constant/uniform buffers to different shader locations.
- Model Viewer - Introduces cameras and camera controls, model loading and inspection.

*Getting started*

To start a new project make sure it is located in pmtech/<project_dir>/ this is important because all paths to tools and other libraries are relative to this location. The examples solution is set up illustrating this layout.. all paths are relative to avoid having to deal with setting environment variables to locate various parts of the project.

An application just needs to define and initialise the pen::window_creation_params struct, and will be given a main loop, which can be defined in a separate project. 

There are helper functions to easily create new projects in tools/premake/app_template.lua and the pen_examples/premake5.lua has examples of how to set up projects. 

Use the make_projects batch file on windows or shell script on osx to generate IDE solutions or workspaces, edit these files to change configuration settings or to see how the current ones work.

The make projects scripts also contain command lines to build shaders and textures for the relevant platform.

supply --help to premake5 for more options on project configuration from inside the .lua files.

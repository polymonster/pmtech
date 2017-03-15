# Welcome to pmtech!

This work is still very much a work in progress, I am currently tidying up the projects and have started on an OSX port with an opengl renderer, the win32 / dx11 is mostly complete and iOS will be next, with metal and vulkan API implementations also on the todo list.

**PEN** *pmtech engine* 

This project constains platform / operating system specific code, it provides abstractions for:
- Renderer
- Window / Entry
- Input / OS Message Pump
- Audio
- Memory
- String (for portability with windows unicode and the vsprintf_s functions)
- File System
- Threads / Syncronisation Primitives
- Timers 
- Job Manager (todo)

The renderer can work in direct mode or deferred mode, direct mode simply wraps up directx (and soon opengl too) into a generic API for platform agnostic use. Deferred mode stores sumbited graphics API calls in a command buffer which is then dispatched on a dedicated thread allowing the graphics driver overhead to be parallelised with other game systems.

Entry point and main loop is abstracted allowing the user to simply specify some simple window creation parameters in an executable project.

**PUT** *pmtech utility toolkit*

This project contains code that will be re-used across different projects but contains no platform specific code, it contains currently:
- "Loader" - Contains functions to load textures (DDS), models, skeletons, animations and shaders
- Bullet physics with a deferred command buffer API like the PEN renderer, so that physics simulate can run independantly on it's own thread.
- Debug renderer helpers, for drawing lines, boxes and other debug primitives
- Simple font renderer (stb font), for debug use
- Scalar (float) Maths library - Vector, Matrix, Quaternion, Intersection tests and helper functions
- ImGui integration (todo)

**Tools**

Tool scripts written in python and using some c++ executables to build data:
- Collada to binary - Models, skeletons and animations
- Textures - Compression and conversion using NVTT (Nvidia)
- Premake5 - All projects are configured using premake and are IDE agnostic
- Shader Compiler - FXC offline shader compilation
- Generic Shader Language (todo)

**pen_examples**

This solution / workspace contains multiple examples of how to use the API's and set up projects, I am currently using them to aid porting, starting with a simple windowed application using minimal dependencies, I will add more examples as I need to implement functionality.

*Getting started*

An application just needs to define and initialise the pen::window_creation_params pen_window struct, and will be given a main loop, which can be defined in a separate project, there are helper functions to easily create in pen/premake_app.lua and the pen_examples/premake5.lua has examples of how to set up and link pen and put.  

Navigate to a project folder, eg pmtech/pen_examples and use "../bin/premake5 <target_name> renderer=<renderer_name> to build project names.

see premake5 --help for more info.

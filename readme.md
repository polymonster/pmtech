# pmtech [![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/github/polymonster/pmtech?branch=master&svg=true&passingText=passing&pendingText=pending&failingText=failing)](https://ci.appveyor.com/project/polymonster/pmtech)

Follow work in progress: 
[Engine](https://trello.com/b/ciujzpUT) | [Editor](https://trello.com/b/PJ76qXKH/editor)

A lightweight code base with powerful features that can be used for games and real-time 3D applications.

The engine (pen) and toolkit (put) are designed with performance in mind. This project focuses on data-oriented code with minimalistic c-style api's. Core systems such as rendering, audio and physics all have dedicated threads which process command buffers generated on the user thread. There are numerous examples included of how to use the api's, set up projects and data:

![sdf shadow](https://polymonster.github.io/assets/gifs/sdf-shadow.gif)
more [demos...](https://polymonster.github.io/demos.html)

**Platform support**  
- Platforms: Windows, MacOS, Linux, iOS (wip).   
- Renderers: Direct3D11, OpenGL3.1+, OpenGLES3+.   
- Compilers: vs2015, vs2017, Clang 6, Apple LLVM 9, Gcc 7. 

**Features**  
- Data-Oriented Component Entity System - handling mesh rendering, animation and transformation heirarchies.
- Editor - Scene, lighting, materials, 3D Volume texture generator and more.
- PMFX - Generic shader language, data driven renderer using JSON config to specify render state, passes and techniques.
- Asset Loading - Models, Textures, Shaders and hot reloading.
- Maths - Templated vector and matrix library, collection of intersection and test functions.
- JSON - Fast minimal json parser for data driven config scripts.
- Platform Agnostic - Renderer, Window, Audio, Memory, Timers, File System, Threads.

**Tools**  
- Models - Convert models, skeletons, scenes and animations to binary format. (Collada, Obj)
- Textures - Compression, conversion, mip-map generation and assembly using NVTT.
- Premake5 - All projects are configured using premake and are IDE agnostic.
- PMFX - shader reflection info and agnostic hlsl / glsl shaders
- Shader Compiler - offline shader compilation.

Read in more detail about pmtech's [features](https://polymonster.github.io/index.html)

**Getting started** 

Take a look at getting started quick [guide](https://polymonster.github.io/index.html#getting-started)

**Shoutout to thirdparty stuff!**  
- [Jsmn](https://github.com/zserge/jsmn)
- [Premake](https://github.com/premake/premake-core)
- [Bullet](https://github.com/bulletphysics/bullet3)
- [ImGui](https://github.com/ocornut/imgui)
- [NVTT](https://github.com/castano/nvidia-texture-tools)

# pmtech [![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/5n3aguiq1ppjrhws?svg=true)](https://ci.appveyor.com/project/polymonster/pmtech) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Follow work in progress: 
[trello](https://trello.com/b/ciujzpUT)

A lightweight code base with powerful features that can be used for games and real-time graphics applications. This project focuses on data-oriented and multithreaded code with minimalistic procedural api's.

Take a look at: 
[demos...](https://polymonster.github.io/index.html)

**Features**  
- Cross Platform: Windows, MacOS, Linux, iOS, Android ([wip](https://trello.com/b/ciujzpUT)).  
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, Metal ([wip](https://trello.com/b/ciujzpUT)).   
- Lightweight - Simple apis, shallow call stacks, minimal dependencies.  
- [Orthodox C++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b) - Sticking to a small c++ subset. 
- Multithreaded - Async render, physics, audio and component entity system.  
- Data-oriented component entity system - SoA memory layout for blazing fast scene representation.
- Pmfx - High level data driven renderer, shader and post-processing system using json.
- Low-level abstractions - Input, gamepad, timers, threads, window, os, file system, etc.
- Hot loading - Reload configs, shaders, models and textures in real time for rapid development.
- Tools - Graphical editor, volume texture / signed distance field generator.
- Asset Pipeline - Binary model and skeleton format, texture compression, platform agnostic shader compilation. 
- Supported Compilers: vs2017, Clang 6, Apple LLVM 9, Gcc 7. 
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information.

**Usage**  

Take a look at the getting started [instructions](https://github.com/polymonster/pmtech/wiki/Getting-Started).

**pmtech uses the following thirdparty libs** 
- [ImGui](https://github.com/ocornut/imgui)
- [Premake](https://github.com/premake/premake-core)
- [Jsmn](https://github.com/zserge/jsmn)
- [Bullet](https://github.com/bulletphysics/bullet3)
- [NVTT](https://github.com/castano/nvidia-texture-tools)
- [stb](https://github.com/nothings/stb)
- [libstem_gamepad](https://github.com/ThemsAllTook/libstem_gamepad)

# pmtech   
[![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/5n3aguiq1ppjrhws?svg=true)](https://ci.appveyor.com/project/polymonster/pmtech) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Follow work in progress: 
[trello](https://trello.com/b/ciujzpUT)

A lightweight code base with powerful features that can be used for games and real-time graphics applications. This project focuses on data-oriented and multithreaded code with minimalistic procedural api's, sticking to [orthodox c++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b) principles.

Take a look at: 
[demos...](https://polymonster.github.io/index.html)

**Features**  
- Cross Platform: Windows, MacOS, Linux, iOS, Android ([wip](https://trello.com/b/ciujzpUT)).  
- Supported Compilers: vs2017, Clang 6, Apple LLVM 9, Gcc 7. 
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, Metal ([wip](https://trello.com/b/ciujzpUT)).   
- Lightweight: Simple apis, shallow call stacks, minimal dependencies.  
- Multithreaded: Async render, physics, audio and entity component system.  
- Data-oriented: Entity component system and root motion animation system.
- Pmfx: High level data driven renderer, shader and post-processing system using json.
- Low-level abstractions: Input, gamepad, timers, threads, window, os, file system, etc.
- Hot loading: Reload configs, shaders, models and textures in real time for rapid development.
- Tools: Graphical editor, volume texture / signed distance field generator.
- [Examples](https://github.com/polymonster/pmtech/wiki/8.-Examples): Lots of examples of how to use the apis from basic usage to rendering techniques (forward, deferred, subsurface scattering, signed distance fields, volume textures), audio, physics and more.
- [Asset Pipeline](https://github.com/polymonster/pmtech/wiki/5.-Build-Pipeline): Binary model and skeleton format, texture compression, platform agnostic shader compilation. 
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information.

**Usage**  

Take a look at the getting started [instructions](https://github.com/polymonster/pmtech/wiki/2.-Building-Examples).

**pmtech uses the following thirdparty libs** 
- [ImGui](https://github.com/ocornut/imgui)
- [Premake](https://github.com/premake/premake-core)
- [Jsmn](https://github.com/zserge/jsmn)
- [Bullet](https://github.com/bulletphysics/bullet3)
- [NVTT](https://github.com/castano/nvidia-texture-tools)
- [stb](https://github.com/nothings/stb)
- [libstem_gamepad](https://github.com/ThemsAllTook/libstem_gamepad)
- [sdf_gen](https://github.com/christopherbatty/SDFGen)

**Media**

[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sss.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/sss/sss.cpp)

[![Renderer](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/pmfx-renderer.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/pmfx_renderer/pmfx_renderer_demo.cpp)

[![Post Processing](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/post-pro.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/post_processing/post_processing.cpp)

[![Vertex Stream Out](https://github.com/polymonster/polymonster.github.io/raw/master/assets/demos/vertex-stream-pbr.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/vertex_stream_out/vertex_stream_out.cpp)


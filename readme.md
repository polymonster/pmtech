# pmtech   
[![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/5n3aguiq1ppjrhws?svg=true)](https://ci.appveyor.com/project/polymonster/pmtech) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Follow work in progress: 
[trello](https://trello.com/b/ciujzpUT)

A lightweight code base with powerful features that can be used for games and real-time graphics applications. This project focuses on data-oriented and multithreaded code with minimalistic procedural api's, sticking to [orthodox c++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b) principles.

**Features**  
- Cross Platform: Windows, MacOS, Linux, iOS, Android ([wip](https://trello.com/b/ciujzpUT)).  
- Supported Compilers: vs2017, Clang 6, Apple LLVM 9, Gcc 7. 
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, Metal. 
- Low-level abstractions: Input, gamepad, timers, threads, window, os, file system, etc.
- Lightweight: Simple apis, shallow call stacks, minimal dependencies.  
- Data-Oriented: Instruction and data cache friendly code for optimal performance. 
- Multithreaded: Async render, physics, audio and entity component system.  
- [Ecs](https://github.com/polymonster/pmtech/wiki/Ecs): Entity component system and root motion animation system.
- [Pmfx](https://github.com/polymonster/pmtech/wiki/Pmfx): High level data driven renderer, shader, compute and post-processing system using json.
- Tools: Graphical editor, volume texture / signed distance field generator.
- [Examples](https://github.com/polymonster/pmtech/wiki/Examples): 30+ samples and unit tests.
- [Build Pipeline](https://github.com/polymonster/pmtech/wiki/Build-Pipeline): IDE project generation, Binary model and skeleton format, texture compression, platform agnostic shader compilation. 
- Future Plans: Direct3D12, Vulkan.
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information.

**Usage**  

Take a look at the getting started [instructions](https://github.com/polymonster/pmtech/wiki/Building-Examples). 
All features in pmtech are demonstarted through [examples](https://github.com/polymonster/pmtech/wiki/Examples). 
Documentation is not thorough but the [source](https://github.com/polymonster/pmtech/wiki/Source-Code) is simple and easy to follow.

**Media**

[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/area-lights.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/area_lights/area_lights.cpp)
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sss.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/sss/sss.cpp)
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sdf-shadow.gif" width="1280" />](https://www.youtube.com/watch?v=369cPinAhdo)
[![Renderer](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/pmfx-renderer.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/pmfx_renderer/pmfx_renderer_demo.cpp)
[![Post Processing](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/post-pro.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/post_processing/post_processing.cpp)
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/stencil-shadows.gif" width="1280" />
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/tourus.gif" width="1280" />
[![Vertex Stream Out](https://github.com/polymonster/polymonster.github.io/raw/master/assets/demos/vertex-stream-pbr.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/vertex_stream_out/vertex_stream_out.cpp)


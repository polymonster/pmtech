# pmtech   
[![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/5n3aguiq1ppjrhws?svg=true)](https://ci.appveyor.com/project/polymonster/pmtech) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Track on https://trello.com/b/05omR9Mj/igneous](https://img.shields.io/badge/track-on_trello-007BC2.svg?logo=trello&logoColor=ffffff&labelColor=026AA7)](https://trello.com/b/ciujzpUT)

A lightweight code base with powerful features that can be used for games and real-time graphics applications.

**Features**
- Lightweight: Minimalistic, simple apis, shallow call stacks. 
- Cross Platform: Windows, macOS, iOS, Linux, Android ([wip](https://trello.com/b/ciujzpUT)).  
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, Metal, Vulkan ([wip](https://trello.com/b/ciujzpUT)).
- Supported Compilers: vs2017+, Clang 6+, Apple LLVM 9+, Gcc 7+. 
- Low-level abstractions: Input, gamepad, timers, threads, window, os, file system, etc. 
- Data-Oriented: Instruction and data cache friendly design for optimal performance. 
- Multithreaded: Async render, physics, audio and entity component system.  
- [Ecs](https://github.com/polymonster/pmtech/wiki/Ecs): Entity component system and root motion animation system.
- [Pmfx](https://github.com/polymonster/pmtech/wiki/Pmfx): High level data driven renderer, shader, compute and post-processing system using json.
- Tools: Graphical editor, volume texture / signed distance field generator.
- [Examples](https://github.com/polymonster/pmtech/wiki/Examples): 30+ samples and unit tests.
- [Build Pipeline](https://github.com/polymonster/pmtech/wiki/Build-Pipeline): IDE project generation, Binary model and skeleton format, texture compression, platform agnostic shader compilation. 
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information.

**Usage**  
- Take a look at the getting started [instructions](https://github.com/polymonster/pmtech/wiki/Building-Examples). 
- All features in pmtech are demonstarted and unit tested through [examples](https://github.com/polymonster/pmtech/wiki/Examples). 
- Documentation is not thorough but the [source](https://github.com/polymonster/pmtech/wiki/Source-Code) is simple and easy to follow.

**Media**  

[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/area-lights.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/area_lights/area_lights.cpp)
<sup>Area Lights.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sss.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/sss/sss.cpp)
<sup>Subsurface Scattering.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sdf-shadow.gif" width="1280" />](https://www.youtube.com/watch?v=369cPinAhdo)
<sup>Signed Distance Field Shadows.</sup>
[![Renderer](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/pmfx-renderer.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/pmfx_renderer/pmfx_renderer_demo.cpp)
<sup>Data Driven, JSON Scriptable Renderer. 100 Lights using Forward, Deferred or Z-Prepass.</sup>
[![Post Processing](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/post-pro.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/post_processing/post_processing.cpp)
<sup>Data Driven, JSON Scriptable Post-Processing. Ray Marched Menger Sponges, Depth of Field, Bloom.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/stencil-shadows.gif" width="1280" />
<sup>Stencil Shadow Volumes.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/tourus.gif" width="1280" />
<sup>64k Data-Oriented Entities, Multiple Shadow Maps, Texture Arrays.</sup>
[![Vertex Stream Out](https://github.com/polymonster/polymonster.github.io/raw/master/assets/demos/vertex-stream-pbr.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/vertex_stream_out/vertex_stream_out.cpp)
<sup>Vertex Stream Out, Instanced Skinning, PBR, Oren Nayar, Cook Torrence.</sup>


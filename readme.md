# pmtech   
[![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/5n3aguiq1ppjrhws?svg=true)](https://ci.appveyor.com/project/polymonster/pmtech) [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Track on https://trello.com/b/05omR9Mj/igneous](https://img.shields.io/badge/track-on_trello-007BC2.svg?logo=trello&logoColor=ffffff&labelColor=026AA7)](https://trello.com/b/ciujzpUT)

**Supported Platforms**
- Operating System: Windows (x64), macOS, iOS, Linux (x64), Android ([wip](https://trello.com/b/ciujzpUT)).  
- Compilers: vs2017+, Clang 6+, Apple LLVM 9+, Gcc 7+. 
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, Metal, Vulkan ([wip](https://trello.com/b/ciujzpUT)).
- Shader Langauges: HLSL Shader Model 3.0+, GLSL 330+, Metal, SPIR-V.

**Features**
- Lightweight: Minimalistic, simple apis, shallow call stacks. 
- Data-Oriented: Instruction and data cache friendly design for optimal performance. 
- Multithreaded: Async render, physics, audio and entity component system.  
- Low-level abstractions: Input, gamepad, timers, threads, window, os, file system, etc. 
- [Ecs](https://github.com/polymonster/pmtech/wiki/Ecs): Entity component system and root motion animation system.
- [Pmfx](https://github.com/polymonster/pmtech/wiki/Pmfx): Scriptable renderer, shader, compute and post-processing system.
- Tools: Graphical editor, volume texture / signed distance field generator.
- [Build Pipeline](https://github.com/polymonster/pmtech/wiki/Build-Pipeline): project generation, compilation, asset building and packaging. 
- [Examples](https://github.com/polymonster/pmtech/wiki/Examples): 40+ samples and unit tests.
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information. 

**Usage**  
- Take a look at the getting started [instructions](https://github.com/polymonster/pmtech/wiki/Building-Examples). 
- All features in pmtech are demonstarted and unit tested through [examples](https://github.com/polymonster/pmtech/wiki/Examples). 

**Media**  

[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/area-lights.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/area_lights/area_lights.cpp)
<sup>Area Lights.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sss.gif" width="1280" />](https://github.com/polymonster/pmtech/blob/master/examples/code/sss/sss.cpp)
<sup>Subsurface Scattering.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sdf-shadow.gif" width="1280" />](https://www.youtube.com/watch?v=369cPinAhdo)
<sup>Signed Distance Field Shadows.</sup>
[![Renderer](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/pmfx-renderer.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/pmfx_renderer/pmfx_renderer_demo.cpp)
<sup>Scriptable Renderer. 100 Lights using Forward, Deferred or Z-Prepass.</sup>
[![Post Processing](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/post-pro.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/post_processing/post_processing.cpp)
<sup>Scriptable Post-Processing. Ray Marched Menger Sponges, Depth of Field, Bloom.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/stencil-shadows.gif" width="1280" />
<sup>Stencil Shadow Volumes.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/tourus.gif" width="1280" />
<sup>64k Data-Oriented Entities, Multiple Shadow Maps, Texture Arrays.</sup>
[![Vertex Stream Out](https://github.com/polymonster/polymonster.github.io/raw/master/assets/demos/vertex-stream-pbr.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/vertex_stream_out/vertex_stream_out.cpp)
<sup>Vertex Stream Out, Instanced Skinning, PBR, Oren Nayar, Cook Torrence.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/shadow-maps.gif" width="1280" />
<sup>Directional, spot and point light shadows.</sup>

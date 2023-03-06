# pmtech  

[![build](https://github.com/polymonster/pmtech/actions/workflows/build.yaml/badge.svg)](https://github.com/polymonster/pmtech/actions/workflows/build.yaml) 
[![tests](https://github.com/polymonster/pmtech/actions/workflows/tests.yaml/badge.svg)](https://github.com/polymonster/pmtech/actions/workflows/tests.yaml) 
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Track on https://trello.com/b/05omR9Mj/igneous](https://img.shields.io/badge/track-on_trello-007BC2.svg?logo=trello&logoColor=ffffff&labelColor=026AA7)](https://trello.com/b/ciujzpUT)  
[![Discord](https://img.shields.io/discord/807665639845789796.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/3yjXwJ8wJC)

Check out the live [WebGL/WebAssembly samples](https://www.polymonster.co.uk/webgl-demos)!

**Supported Platforms**  

- Operating System: Windows (x64), macOS, iOS, Linux (x64), Web Assembly, Android ([wip](https://trello.com/b/ciujzpUT)).
- Compilers: vs2017+, Clang 6+, Apple LLVM 9+, Gcc 7+, emcc 2.0. 
- Rendering Backends: Direct3D11, OpenGL3.1+, OpenGLES3+, WebGL 2.0, Metal, Vulkan ([wip](https://trello.com/b/ciujzpUT)).
- Shader Langauges: HLSL Shader Model 3.0+, GLSL 330+, Metal, SPIR-V.

**Features**  

- Lightweight: Minimalistic, simple apis, shallow call stacks.  
- Data-Oriented: Instruction and data cache friendly design for optimal performance.  
- Multithreaded: Async render, physics, audio and entity component system.  
- Low-level abstractions: Input, gamepad, timers, threads, window, os, file system, etc.  
- [Live-reloading](https://www.youtube.com/watch?v=dSLwP4D8Fd4): Dynamically reload c++, shaders and render pipleines.
- [Ecs](https://github.com/polymonster/pmtech/tree/master/core/put/source/ecs): Entity component system and root motion animation system.
- [Pmfx](https://github.com/polymonster/pmtech/wiki/Pmfx): Scriptable renderer, shader, compute and post-processing system.
- [Tools](https://github.com/polymonster/pmtech/wiki/Building): Graphical editor, mesh optimiser, volume texture / sdf generator.
- [Build Pipeline](https://github.com/polymonster/pmtech/wiki/Building): project generation, compilation, asset building and packaging.  
- [Examples](https://github.com/polymonster/pmtech/wiki/Examples): 40+ samples and unit tests.
- [Example Game](https://github.com/polymonster/dr_scientist): Dr. Scientist is a demo game using pmtech.
- Visit the [wiki](https://github.com/polymonster/pmtech/wiki) for more information.  

**Usage**  

- Take a look at the [Building](https://github.com/polymonster/pmtech/wiki/Building) instructions to get started.  
- All features in pmtech are demonstarted and unit tested through [examples](https://github.com/polymonster/pmtech/wiki/Examples). 

**Media**  

[<img src="https://github.com/polymonster/polymonster.github.io/blob/da8757c5d9e8a142f0f4ef4a83c486109467e7c1/images/pmtech/gifs/dr_scientist.gif" width="100%" />](https://github.com/polymonster/dr_scientist)
<sup>Dr. Scientist. - an example game with root motion animation and kinematic physics character controller</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/gi_demo.gif" width="100%" />](https://github.com/polymonster/pmtech/blob/master/examples/code/global_illumincation/global_illumincation.cpp)
<sup>Global Illumination + Temporal Anti-Aliasing.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/area-lights.gif" width="100%" />](https://github.com/polymonster/pmtech/blob/master/examples/code/area_lights/area_lights.cpp)
<sup>Area Lights.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sss.gif" width="100%" />](https://github.com/polymonster/pmtech/blob/master/examples/code/sss/sss.cpp)
<sup>Subsurface Scattering.</sup>
[<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/sdf-shadow.gif" width="100%" />](https://www.youtube.com/watch?v=369cPinAhdo)
<sup>Signed Distance Field Shadows.</sup>
[![Renderer](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/pmfx-renderer.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/pmfx_renderer/pmfx_renderer_demo.cpp)
<sup>Scriptable Renderer. 100 Lights using Forward, Deferred or Z-Prepass.</sup>
[![Post Processing](https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/post-pro.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/post_processing/post_processing.cpp)
<sup>Scriptable Post-Processing. Ray Marched Menger Sponges, Depth of Field, Bloom.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/stencil-shadows.gif" width="100%" />
<sup>Stencil Shadow Volumes.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/tourus.gif" width="100%" />
<sup>64k Data-Oriented Entities, Multiple Shadow Maps, Texture Arrays.</sup>
[![Vertex Stream Out](https://github.com/polymonster/polymonster.github.io/raw/master/assets/demos/vertex-stream-pbr.gif)](https://github.com/polymonster/pmtech/blob/master/examples/code/vertex_stream_out/vertex_stream_out.cpp)
<sup>Vertex Stream Out, Instanced Skinning, PBR, Oren Nayar, Cook Torrence.</sup>
<img src="https://github.com/polymonster/polymonster.github.io/raw/master/assets/gifs/shadow-maps.gif" width="100%"/>
<sup>Directional, spot and point light shadows.</sup>

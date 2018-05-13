# pmtech [![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/github/polymonster/pmtech?branch=master&svg=true&passingText=passing&pendingText=pending&failingText=failing)](https://ci.appveyor.com/project/polymonster/pmtech)

Follow work in progress: 
[Engine](https://trello.com/b/ciujzpUT) | [Editor](https://trello.com/b/PJ76qXKH/editor)

A lightweight code base with powerful features that can be used for games, 3d and real-time applications.

The engine (pen) and toolkit (put) are designed with simplicitiy in mind, c-style api's, minimal amounts of code and data-oriented design are the philosophy behind this project. Core systems such as rendering, audio and physics all have dedicated threads which process command buffers generated on the user thread.  

**Support**  
- Platforms: Windows, MacOS, Linux (wip), iOS (wip).   
- Renderers: Direct3D11, OpenGL3.1+, OpenGLES3+.   
- Compilers: vs2015, vs2017, Clang 6, Apple LLVM 9, Gcc 7. 

**Features**  
- Cross Platform - Renderer, Window, Audio, Memory, Timers, File System, Threads.
- Model, Texture, Shader loading and hot reloading.
- Maths Library. 
- JSON - Fast minimal json parser for data driven config scripts.
- PMFX - Generic shader language, data driven renderer using JSON config to specify render state, passes and techniques.
- Data-Oriented Component Entity System - handling mesh rendering, animation and transformation heirarchies.
- Editor - Scene, lighting, materials, 3D Volume texture generator and more.

**Tools / Build Scripts**  
- Models - Convert Collada models, skeletons and animations to binary format.
- Textures - Compression, conversion, mip-map generation and assembly using NVTT.
- Premake5 - All projects are configured using premake and are IDE agnostic.
- PMFX - shader reflection info and agnostic hlsl / glsl shaders
- Shader Compiler - offline shader compilation.

**Thirdparty Stuff**  
- [Jsmn](https://github.com/zserge/jsmn)
- [Premake](https://github.com/premake/premake-core)
- [Bullet](https://github.com/bulletphysics/bullet3)
- [ImGui](https://github.com/ocornut/imgui)
- [NVTT](https://github.com/castano/nvidia-texture-tools)

**Getting started**  
requires python3  
osx / linux
```bash
pmtech cd examples
pmtech/examples python3 ../tools/build.py -all
```

ios
```bash
pmtech cd examples
pmtech/examples python3 ../tools/build.py -all -platform ios
```

win32
```cmd
pmtech cd examples
pmtech\examples py -3 ..\tools\build.py -all
```

**Examples**   
This workspace contains multiple examples of how to use the API's and set up projects, they are also useful to aid porting and test functionality. The samples also help catch errors when they continuously integrate changes using tavis and appveyor.

- empty_project - First port of call to get a platform compiling, it creates an empty window with no rendering context.
- basic_traingle - introduces a rendering context, clear sceen, shader loading / binding, vertex buffer and non-indexed draw calls.
- debug_font - introduces shader loading with the "put" library, vertex buffer updating and debug text rendering.
- basic_texture - Introduces texture loading using the "put" library, index buffers, indexed draw calls, texture samplers and texture binding.
- render_target - Introduces render target creation / binding, shader program loading using the "put" library to automatically generate input layouts from the shaders input signature.
- play_sound - Introduces linking to fmod 5 and some basic audio functions.
- imgui - Displays the Imgui test example, this also introduces a few new rendering festures that are required, blending, depth stencil states and scissor testing.
- audio_player - Introduces more audio features and supplies a UI to play and control audio files, it also introduces fft and some beat-detection code along with graphs and visualisations of the fft this is r&d work in progress.
- shader_toy - Introduces shader hot loading and a test bed for binding textures, samplers and constant/uniform buffers to different shader locations.
- instancing - Intorduces instanced draw calls.
- rigid_bodies - Introduces basic rigid body primitives using bullet physics and rendering primitives.
- constraints - Introduces six degrees of freedom, hinge and point to point constraints.
- shadows (wip) - Demonstrates how pmfx can be setup to render a scene from multiple views into different render targets and implements shadow maps, cascaded shadow maps and cube shadow maps for point lights.
- skinning - Introduces skinned vertex formats, animation controllers and animations in the component entity scene.
- cubemap - Introduces cubemap loading and basic rendering.
- texture_formats - Loads and displays a number of compressed and uncompressed texture formats with mip maps.
- scene_editor - An editor to edit the data-oritented c-style component entity system.
- vertex_stream_out - Uses vertex stream out / transform feedback to skin an animated character once per frame, the resulting vertex buffer that has been written from the vertex shader is then rendered many times with instancing to demostrate how vertex stream out can avoid re-skinning a model many times during different render passes or draw call instances.
- volume_texture - Demonstrates loading and rendering of 3d volume textures.
- multiple_render_targets - Renders a simple scene into multiple output buffers (albedo and normals), to illustrate how to use pmfx to setup deferred rendering or other rendering strategies which require mrt.

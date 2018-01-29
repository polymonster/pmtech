# Welcome to pmtech! [![Build Status](https://travis-ci.org/polymonster/pmtech.svg?branch=master)](https://travis-ci.org/polymonster/pmtech) [![Build Status](https://ci.appveyor.com/api/projects/status/github/polymonster/pmtech?branch=master&svg=true&passingText=win32-passing&pendingText=win32-pending&failingText=win32-failing)](https://ci.appveyor.com/project/polymonster/pmtech)

Follow work in progress: 
[Engine](https://trello.com/b/ciujzpUT) | [Editor](https://trello.com/b/PJ76qXKH/editor)

A lightweight code base with powerful features that can be used for games, 3d and real-time applications. It offers cross platform support for osx, win32 and ios using opengl 3 and directx 11 renderers.

Core systems such as rendering, audio and physics are handled asyncronously on consumer threads which process command buffers that are created on the user thread, the command buffer api's provide thread safe access to add commands or read data back from a consumer thread.

The engine and toolkit are designed with simplicitiy in mind, c-style api's and data-oriented design are the philosophy behind this project, with minimal use of c++ features just for convenience.

**Features**  
- Cross Platform - Renderer, Window, Audio, Memory, Timers, File System, Threads.
- Model, Texture, Shader loading.
- Resource hot loading.
- Maths Library. 
- JSON - Fast minimal json parser for data driven config scripts.
- PMFX - Generic shader language with GPU state specification for setting blend, depth, stencil, raster and other GPU state. PMFX is also a data driven renderer where a JSON config can be used to define render passes, target scene cameras to render targets, select shader techniques and specify GPU state.
- Component Entity System - Data-Oriented, written in c+, handling mesh rendering, skeletal animation and scene and transformation heirarchies.
- Scene Editor - Scene editor built on the Component Entity Scene.

**Thirdparty Stuff**  
- [Jsmn](https://github.com/zserge/jsmn)
- [Premake](https://github.com/premake/premake-core)
- [Bullet](https://github.com/bulletphysics/bullet3)
- [ImGui](https://github.com/ocornut/imgui)

**Tools**  
Tool scripts written in python and some executables are used to build data:
- Collada to Binary - Models, skeletons and animations.
- Textures - Compression and conversion using NVTT (Nvidia).
- Premake5 - All projects are configured using premake and are IDE agnostic.
- PMFX - shader reflection info and agnostic hlsl / glsl shaders
- Shader Compiler - offline shader compilation.

**Getting started**  
Run the tools/build.py script from pmtech/examples to see how to build code projects and data.     
on osx you can run ./travis.sh which will genereate GNU make files and compile from the command line.

**Examples**   
This solution / workspace contains multiple examples of how to use the API's and set up projects, I have been using them to aid porting, starting with a simple windowed application using minimal dependencies, samples introduce more dependancies as they go along, this would also be an ideal place to add some unit tests and continously test functionality of the engine.

- empty_project - First port of call to get a platform compiling, it creates an empty window with no rendering context.
- basic_traingle - introduces a rendering context, clear sceen, shader loading / binding, vertex buffer and non-indexed draw calls.
- debug_font - introduces shader loading with the "put" library, vertex buffer updating and debug text rendering.
- textures - Introduces texture loading using the "put" library, index buffers, indexed draw calls, texture samplers and texture binding.
- render_target - Introduces render target creation / binding, shader program loading using the "put" library to automatically generate input layouts from the shaders input signature.
- play_sound - Introduces linking to fmod 5 and some basic audio functions.
- imgui - Displays the Imgui test example, this also introduces a few new rendering festures that are required, blending, depth stencil states and scissor testing.
- audio_player - Introduces more audio features and supplies a UI to play and control audio files, it also introduces fft and some beat-detection code along with graphs and visualisations of the fft this is r&d work in progress.
- shader_toy - Introduces shader hot loading and a test bed for binding textures, samplers and constant/uniform buffers to different shader locations.
- instancing - Intorduces instanced draw calls.
- rigid_bodies - Introduces basic rigid body primitives using bullet physics and rendering primitives.
- constraints - Introduces six degrees of freedom, hinge and point to point constraints.
- scene_editor - Introduces cameras and camera controls, model loading and inspection, component entity system - simple c-style component entity system using structure of arrays layout for efficient cache utilisation.. a "scene" design pattern that is not object oriented but can be just as powerful.

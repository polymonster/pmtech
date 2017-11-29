# Welcome to pmtech!

A lightweight code base with powerful features that can be used for games, 3d and real-time applications. It offers cross platform support for osx, win32 and ios with opengl 3 and directx 11 renderers with metal, directx 12 and more in the pipeline.

Core systems such as rendering, audio and physics are handled asyncronously on consumer threads which process command buffers that are created on the user thread, the command buffer api's provide thread safe access to add commands or read data back from a consumer thread.

The engine and toolkit are designed with simplicitiy in mind c-style api's and data-oriented programming are the philosophy behind this project, with minimal use of c++ features for convenience.

**pen** *pmtech engine* 

This project contains platform / operating system specific code, it provides abstractions for:

Renderer, Window, Audio, Memory, Timers, File System, Threads.

**put** *pmtech utility toolkit*

This project contains code that will be re-used across different projects but contains no platform specific code:
- Model, Texture and Shader loading.
- Dev UI, Maths Library, Async Physics command buffer API.
- JSON - Simple c json parser greate for config and data files. 
- PMFX - generic shader language with GPU state specification for setting blend, depth, stencil, raster and other GPU state.
- Render Controller - Scriptable renderer to define render passes and GPU state from in a JSON config.
- Component Entity Scene - Data-Oriented scene written in c+, handling mesh rendering, skeletal animation and scene heirarchies.
- Editor - Scene editor built on the Component Entity Scene.

**thirdparty stuff**

- [a ImGui](https://github.com/ocornut/imgui)
- [a Str](https://github.com/ocornut/str)
- [a Jsmn](https://github.com/zserge/jsmn)
- [a Bullet](https://github.com/bulletphysics/bullet3)

**tools**
Tool scripts written in python and using some c++ executables to build data:
- Collada to Binary - Models, skeletons and animations.
- Textures - Compression and conversion using NVTT (Nvidia).
- Premake5 - All projects are configured using premake and are IDE agnostic.
- ios project genetion - Simple script to copy ios files and fixup xcode to support ios projects.
- Shader Compiler - FXC offline shader compilation.
- Shader Builder - A python script and some macros help porting from hlsl to glsl, JSON can be used to specify addition information such as depth stencil state or blend modes. A JSON metadata file is generated along with each shader program to provide reflection information to help generate d3d input layouts, gl vertex arrays and bind gl uniforms, uniform buffers and textures to the correct locations.

*Getting started*

Run the build.py script in pmtech/examples to see how to build code projects and data

on osx you can run ./travis.sh which will genereate GNU make files and compile from the command line.

**examples**

This solution / workspace contains multiple examples of how to use the API's and set up projects, I have been using them to aid porting, starting with a simple windowed application using minimal dependencies, samples introduce more dependancies as they go along, this would also be an ideal place to add some unit tests and continously test functionality of the engine.

- empty_project - First port of call to get a platform compiling, it creates an empty window with no rendering context.
- basic_traingle - introduces a rendering context, clear sceen, shader loading / binding, vertex buffer and non-indexed draw calls.
- debug_font - introduces shader loading with the "put" library, vertex buffer updating and debug text rendering.
- textures - Introduces texture loading using the "put" library, index buffers, indexed draw calls, texture samplers and texture binding.
- render_target - Introduces render target creation / binding, shader program loading using the "put" library to automatically generate input layouts from the shaders input signature.
- play_sound - Introduces linking to fmod 5 and some basic audio functions.
- Imgui - Displays the Imgui test example, this also introduces a few new rendering festures that are required, blending, depth stencil states and scissor testing.
- Audio Player - Introduces more audio features and supplies a UI to play and control audio files, it also introduces fft and some beat-detection code along with graphs and visualisations of the fft this is r&d work in progress.
- Shader Toy - Introduces shader hot loading and a test bed for binding textures, samplers and constant/uniform buffers to different shader locations.
- Model Viewer - Introduces cameras and camera controls, model loading and inspection
- Component entity system - simple c-style component entity system using structure of arrays layout for efficient cache utilisation.. a "scene" design pattern that is not object oriented but can be just as powerful.

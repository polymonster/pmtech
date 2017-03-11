This is my game engine framework I have been using to develop and prototype ideas:

This work is still in progress, I am currently tidying up the projects and will be adding an OSX port, with opengl renderer.

Here is a quick break down of how it works and how it is laid out:

PEN
This project constains platform specific code, it provides abstraction for input, window creation, rendering, audio and a few other things.

The renderer can work in direct mode or deferred mode, direct mode simply wraps up directx (and soon opengl too) into a generic API for platform agnostic use.

The renderer can also work in deferred mode where all user sumbited graphics API calls are stored in a command buffer which is then sumbitted on a dedicated thread allowing the graphics driver overhead to be parallelised with other game systems.

Audio also will have a command buffered deferred API which is currently work in progress.

PUT
This project contains code that will be re-used across different projects but contains no platform specific code, it contains currently:
- Texture and model loaders.
- Skeletal animation loader.
- Shader loading.
- Bullet physics with a deferred command buffer API like the PEN renderer, so that physics simulate can run independantly on it's own thread.
- Debug renderer helpers, for drawing lines, boxes and other debug primitives.
- Simple font renderer, for debug use.
- ImGui integration (WIP)

Tools
Python scripts to parse collada and create bespoke binary model format, compress textures and compile shaders offline.
uses NVTT for the texture compression
all projects use premake for quick and simple setup, no project files are commited to the repository.

Examples
This project has some examples of how to use the API's and set up projects.

An application just needs to definine the pen::game_entry function and some externals for window creation, this means starting a new fresh project is simple.

Getting started
Navigate to the examples folder in a command prompt or terminal
type ../bin/premake5 <target_name> to build projects

target_names:
vs2013
vs2015
xcode

 
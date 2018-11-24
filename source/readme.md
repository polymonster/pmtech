
pmtech source is split into two libraries:

# pen
Contains platform / operating system specific code and other utility code which is used at this low level. It serves mainly as an abstraction layer for cross platform support, it has the following modules:

- Renderer
- Types
- Timer
- Threads
- Console
- Slot Resource
- Json
- String
- Input
- Memory

# put
Contains only platform agnostic code which is implemented by using the abstractions provided by pen, it has the following modules:

- Component Entity System +Editor
- PMFX (High level data driven renderer and post-processing system)
- Volume Generator
- Audio
- Physics
- Debug Renderer
- Asset Loading / Management
- Dev UI (ImGui renderer +Extensions)


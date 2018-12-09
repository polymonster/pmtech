
pmtech source is split into two static libraries

## pen
Contains platform system specific code and utility code which is required at this low level, it contains the following modules:

- Renderer
- OS
- Filesystem
- Types
- Memory
- Timer
- Threads
- Console
- Slot Resource
- Json
- String
- Input
- Hash
- Data Structures

## put
Contains only platform agnostic code which is implemented by using the abstractions provided by pen, it contains the following modules:

- Component Entity System +Editor
- PMFX (High level data driven renderer and post-processing system)
- Volume Generator
- Audio
- Physics
- Debug Renderer
- Asset Loading / Management
- Dev UI (ImGui renderer +Extensions)


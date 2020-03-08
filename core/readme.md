
pmtech is split into 2 static libraries, **pen** and **put**

Read more about the source code and coding philosophy on this project [here](https://github.com/polymonster/pmtech/wiki/Source-Code).

**pen** contains platform specific code and abstractions for the following:
- [console](https://github.com/polymonster/pmtech/blob/master/source/pen/include/console.h)
- [data_struct](https://github.com/polymonster/pmtech/blob/master/source/pen/include/data_struct.h)
- [file_system](https://github.com/polymonster/pmtech/blob/master/source/pen/include/file_system.h)
- [hash](https://github.com/polymonster/pmtech/blob/master/source/pen/include/hash.h)
- [input](https://github.com/polymonster/pmtech/blob/master/source/pen/include/input.h)
- [memory](https://github.com/polymonster/pmtech/blob/master/source/pen/include/memory.h)
- [os](https://github.com/polymonster/pmtech/blob/master/source/pen/include/os.h)
- [pen_json](https://github.com/polymonster/pmtech/blob/master/source/pen/include/pen_json.h)
- [pen_string](https://github.com/polymonster/pmtech/blob/master/source/pen/include/pen_string.h)
- [renderer](https://github.com/polymonster/pmtech/blob/master/source/pen/include/renderer.h)
- [str_utilities](https://github.com/polymonster/pmtech/blob/master/source/pen/include/str_utilities.h)
- [types](https://github.com/polymonster/pmtech/blob/master/source/pen/include/types.h)
- [timer](https://github.com/polymonster/pmtech/blob/master/source/pen/include/timer.h)
- [threads](https://github.com/polymonster/pmtech/blob/master/source/pen/include/threads.h)

**put** contains platform agnostic code using the abstractions provided by pen, it contains the following modules:
- [audio](https://github.com/polymonster/pmtech/blob/master/source/put/source/audio.h)
- [camera](https://github.com/polymonster/pmtech/blob/master/source/put/source/camera.h)
- [debug_render](https://github.com/polymonster/pmtech/blob/master/source/put/source/debug_render.h)
- [dev_ui](https://github.com/polymonster/pmtech/blob/master/source/put/source/dev_ui.h)
- [entity component system](https://github.com/polymonster/pmtech/tree/master/source/put/source/ecs)
- [loader](https://github.com/polymonster/pmtech/blob/master/source/put/source/loader.h)
- [pmfx](https://github.com/polymonster/pmtech/blob/master/source/put/source/pmfx.h)
- [physics](https://github.com/polymonster/pmtech/blob/master/source/put/source/physics.h)
- [volume_generator](https://github.com/polymonster/pmtech/blob/master/source/put/source/volume_generator.h)

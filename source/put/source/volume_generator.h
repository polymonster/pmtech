// volume_generator.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef _volume_generator_h
#define _volume_generator_h

namespace put
{
    namespace ces
    {
        struct entity_scene;
    }

    namespace vgt
    {
        void init(ces::entity_scene* scene);

        void show_dev_ui();
        void post_update();
    } // namespace vgt
} // namespace put

#endif //_volume_generator_h

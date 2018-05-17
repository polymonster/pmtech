#ifndef _loader_h
#define _loader_h

#include "pen.h"
#include "str/Str.h"
#include <vector>

namespace pen
{
    struct texture_creation_params;
}

namespace put
{
    typedef pen::texture_creation_params texture_info;

    // Textures
    u32 load_texture(const c8 *filename);
    void save_texture(const c8 *filename, const texture_info &tcp);
    void get_texture_info(u32 handle, texture_info &info);

    // Hot loading
    Str get_build_cmd();
    void add_file_watcher(const c8 *filename, void (*build_callback)(),
                          void (*hotload_callback)(std::vector<hash_id> &dirty));
    void poll_hot_loader();
} // namespace put

#endif

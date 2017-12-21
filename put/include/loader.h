#ifndef _loader_h
#define _loader_h

#include "definitions.h"
#include <vector>
#include "str/Str.h"

namespace put
{
    u32 load_texture( const c8* filename );

	Str get_build_cmd();

	void add_file_watcher(const c8* filename, void(*build_callback)(), void(*hotload_callback)(std::vector<hash_id>& dirty));

	void poll_hot_loader();
}

#endif

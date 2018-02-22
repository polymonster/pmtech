#ifndef _loader_h
#define _loader_h

#include "definitions.h"
#include <vector>
#include "str/Str.h"

namespace pen
{
	struct texture_creation_params;
}

namespace put
{
	typedef pen::texture_creation_params texture_info;

    u32 load_texture( const c8* filename );

	void get_texture_info( u32 handle, texture_info& info );

	Str get_build_cmd();

	void add_file_watcher(const c8* filename, void(*build_callback)(), void(*hotload_callback)(std::vector<hash_id>& dirty));

	void poll_hot_loader();
}

#endif

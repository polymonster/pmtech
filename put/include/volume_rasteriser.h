#ifndef _volume_rasteriser_h
#define _volume_rasteriser_h

namespace put
{
	namespace ces
	{
		struct entity_scene;
	}

	namespace vrt
	{
		void init(ces::entity_scene* scene);
		void shutdown();

		void show_dev_ui();
	}
}

#endif //_volume_rasteriser_h
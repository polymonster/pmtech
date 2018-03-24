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
		void shutdown();

		void show_dev_ui();
	}
}

#endif //_volume_generator_h
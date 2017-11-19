#include "render_controller.h"
#include "entry_point.h"
#include "component_entity.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "pen_json.h"
#include "file_system.h"

namespace put
{
    namespace render_controller
    {
        void parse_render_targets( pen::json& render_config )
        {
            pen::json j_render_targets = render_config["render_targets"];
        }
        
        void parse_views( pen::json& render_config )
        {
            pen::json j_views = render_config["views"];
        }
        
        void init( const c8* filename )
        {
            void* config_data;
            u32   config_data_size;
            
            pen_error err = pen::filesystem_read_file_to_buffer(filename, &config_data, config_data_size);
            
            if( err != PEN_ERR_OK || config_data_size == 0 )
            {
                //failed load file
                pen::memory_free(config_data);
                PEN_ASSERT(0);
            }
            
            pen::json render_config = pen::json::load((const c8*)config_data);
            
            PEN_PRINTF( render_config.dumps().c_str() );
            
            parse_render_targets(render_config);
            
            parse_views(render_config);
        }
    }
}

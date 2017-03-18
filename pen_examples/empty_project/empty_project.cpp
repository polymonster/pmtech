#include "pen.h"
#include "threads.h"
#include "filesystem.h"

pen::window_creation_params pen_window
{
	1280,					//width
	720,					//height
	4,						//MSAA samples
	"empty_project"         //window title / process name
};

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    filesystem_enumeration results;
    pen::filesystem_enum_directory(L"/Users/alex.dixon/ö_ppppp", results);
    
    for( ;; )
    {
        pen::string_output_debug("oh hai ö\n");
        
        pen::threads_sleep_us(16000);
    }
    
	return PEN_OK;
}

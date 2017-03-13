#include "pen.h"

pen::window_creation_params pen_window
{
	1280,					//width
	720,					//height
	4,						//MSAA samples
	"empty_project"         //window title / process name
};

PEN_THREAD_RETURN pen::game_entry( void* params )
{
	return 0;
}

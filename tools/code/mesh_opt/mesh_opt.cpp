#include "ecs/ecs_resources.h"

#include "console.h"
#include "file_system.h"
#include "pen.h"
#include "threads.h"
#include "os.h"

using namespace pen;
using namespace put;
using namespace ecs;

void* pen::user_entry(void* params);

static Str* s_args = nullptr;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        // unpack args
        for(u32 i = 0; i < argc; ++i)
            sb_push(s_args, argv[i]);
            
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "mesh_optimiser";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::console_app;
        return p;
    }
} // namespace pen

void show_help()
{
    PEN_LOG("mesh_opt help");
    PEN_LOG("    -help <show this dialog>");
    PEN_LOG("    -i <input file>");
    PEN_LOG("    -o (optional) <output file>");
    PEN_LOG("      if -o is not supplied input file will be overwritten in place.");
}

void* pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);
    
    Str input_file = "";
    Str output_file = "";
    
    u32 argc = sb_count(s_args);
    for(u32 i = 0; i < argc; ++i)
    {
        if(s_args[i] == "-help")
        {
            break;
        }
        else if(s_args[i] == "-i" && i+1 < argc)
        {
            input_file = s_args[i+1];
        }
        else if(s_args[i] == "-o" && i+1 < argc)
        {
            output_file = s_args[i+1];
        }
    }
    
    if(input_file.empty())
    {
        show_help();
        goto term;
    }
    
    // write in place
    if(output_file.empty())
    {
        output_file = input_file;
    }
    
    PEN_LOG("optimising: %s", input_file.c_str());
    optimise_pmm(input_file.c_str(), output_file.c_str());
    
term:
    // signal to the engine the thread has finished
    pen::os_terminate(0);
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}

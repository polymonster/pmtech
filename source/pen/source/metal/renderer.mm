#import <MetalKit/MetalKit.h>
#import <Metal/Metal.h>

#include "renderer.h"
#include "console.h"
#include "data_struct.h"

// globals
a_u8 g_window_resize;

#define MAX_VB 4 // max simulataneously bound vertex buffers

namespace
{
    struct clear_cmd
    {
        u32 clear_state;
        u32 colour_index;
        u32 depth_index;
    };
    
    struct vertex_buffer_cmd
    {
        u32 buffer_index[MAX_VB];
        u32 stride[MAX_VB];
        u32 offset[MAX_VB];
        u32 num_buffers;
        u32 start_slot;
    };
    
    struct current_state
    {
        // metal
        id<MTLRenderPipelineState>  pipeline;
        id<MTLCommandQueue>         command_queue;
        id<MTLRenderCommandEncoder> render_encoder;
        id<MTLCommandBuffer>        cmd_buffer;
        id<CAMetalDrawable>         drawable;
        
        // pen
        pen::viewport               viewport;
        u32                         index_buffer;
        u32                         vertex_shader;
        u32                         fragment_shader;
        clear_cmd                   clear;
        vertex_buffer_cmd           vertex_buffers;
    };
    
    struct buffer_resource
    {
        id<MTLBuffer> buf;
    };
    
    struct shader_resource
    {
        id <MTLLibrary> lib;
        u32             type;
    };
    
    struct metal_clear_state
    {
        MTLLoadAction colour_load_action;
        MTLClearColor colour[pen::MAX_MRT];
    };
    
    struct resource
    {
        union
        {
            buffer_resource     buffer;
            shader_resource     shader;
            metal_clear_state   clear;
        };
    };
    
    resource*       s_resource_pool = nullptr;
    
    MTKView*        s_metal_view;
    id<MTLDevice>   s_metal_device;
    current_state   s_current_state;
}

pen_inline void pool_grow(resource* pool, u32 size)
{
    
}

namespace pen
{
    a_u64 g_gpu_total;
    
    // renderer specific info
    bool renderer_viewport_vup()
    {
        return false;
    }
    
    const c8* renderer_get_shader_platform()
    {
        return "metal";
    }

    const renderer_info& renderer_get_info()
    {
        static renderer_info info;
        
        return info;
    }
    
    namespace direct
    {
        void bind_state()
        {
            if(s_current_state.render_encoder != 0)
                return;
            
            // rt and clear.. create render_encoder
            MTLRenderPassDescriptor *pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLCommandBuffer> cmd_buffer = [s_current_state.command_queue commandBuffer];
            
            
            metal_clear_state& clear = s_resource_pool[s_current_state.clear.clear_state].clear;
            
            id<CAMetalDrawable> drawable = s_metal_view.currentDrawable;
            id<MTLTexture> texture = drawable.texture;
            
            pass_desc.colorAttachments[0].texture = texture;
            pass_desc.colorAttachments[0].loadAction = clear.colour_load_action;
            pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass_desc.colorAttachments[0].clearColor = clear.colour[0];
            
            id <MTLRenderCommandEncoder> render_encoder = [cmd_buffer renderCommandEncoderWithDescriptor:pass_desc];
            
            // create pipeline
            MTLRenderPipelineDescriptor *pipeline_desc = [MTLRenderPipelineDescriptor new];
            
            shader_resource& vs_res = s_resource_pool[s_current_state.vertex_shader].shader;
            shader_resource& fs_res = s_resource_pool[s_current_state.fragment_shader].shader;
            
            id<MTLFunction> vs_main = [vs_res.lib newFunctionWithName:@"vs_main"];
            id<MTLFunction> fs_main = [fs_res.lib newFunctionWithName:@"ps_main"];
            
            pipeline_desc.vertexFunction = vs_main;
            pipeline_desc.fragmentFunction = fs_main;
            pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            
            NSError *error = nil;
            id<MTLRenderPipelineState> pipeline = [s_metal_device newRenderPipelineStateWithDescriptor:pipeline_desc
                                                                                                 error:&error];
            
            [render_encoder setRenderPipelineState:pipeline];

            vertex_buffer_cmd& vb = s_current_state.vertex_buffers;
            for(u32 i = 0; i < vb.num_buffers; ++i)
            {
                [render_encoder setVertexBuffer:s_resource_pool[vb.buffer_index[i]].buffer.buf
                                         offset:vb.offset[i]
                                        atIndex:vb.start_slot+i];
            }
            
            s_current_state.render_encoder = render_encoder;
            s_current_state.cmd_buffer = cmd_buffer;
            s_current_state.drawable = drawable;
        }
        
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res) 
        {
            s_metal_view = (MTKView*)params;
            s_metal_device = s_metal_view.device;
            
            s_current_state.command_queue = [s_metal_device newCommandQueue];
            s_current_state.render_encoder = 0;
            
            //reserve space for some resources
            sb_grow(s_resource_pool, 100);
            
            return 1;
        }
        
        void renderer_shutdown() 
        {
            
        }
        
        void renderer_make_context_current() 
        {
            // stub. used for gl implementation
        }
        
        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot) 
        {
            //pool_grow(s_resource_pool, resource_slot);
            
            metal_clear_state& mc = s_resource_pool[resource_slot].clear;
            
            // flags
            mc.colour_load_action = MTLLoadActionLoad;
            if(cs.flags & PEN_CLEAR_COLOUR_BUFFER)
                mc.colour_load_action = MTLLoadActionClear;
            
            // colour
            mc.colour[0] = MTLClearColorMake(cs.r, cs.g, cs.b, cs.a);
            
            for(u32 i = 0; i < cs.num_colour_targets; ++i)
            {
                const mrt_clear& m = cs.mrt[i];
                
                switch(m.type)
                {
                    case CLEAR_F32:
                        mc.colour[i] = MTLClearColorMake(m.rf, m.gf, m.bf, m.af);
                        break;
                    case CLEAR_U32:
                        mc.colour[i] = MTLClearColorMake(m.ri, m.gi, m.bi, m.ai);
                        break;
                }
            }
            
            // depth stencil
        }
        
        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {
            s_current_state.clear = { clear_state_index, colour_face, depth_face };
        }
        
        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot) 
        {
            //pool_grow(s_resource_pool, resource_slot);
            
            const c8* csrc = (const c8*)params.byte_code;
            NSString* str = [[NSString alloc] initWithBytes:csrc length:params.byte_code_size
                                                   encoding:NSASCIIStringEncoding];
            
            NSError* err = nil;
            MTLCompileOptions* opts = [MTLCompileOptions alloc];
            id <MTLLibrary> lib = [s_metal_device newLibraryWithSource:str options:opts error:&err];
            
            s_resource_pool[resource_slot].shader = { lib, params.type };
        }
        
        void renderer_set_shader(u32 shader_index, u32 shader_type) 
        {
            switch(shader_type)
            {
                case PEN_SHADER_TYPE_VS:
                    s_current_state.vertex_shader = shader_index;
                    break;
                case PEN_SHADER_TYPE_PS:
                    s_current_state.fragment_shader = shader_index;
                    break;
            };
        }
        
        void renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_input_layout(u32 layout_index) 
        {
        
        }
        
        void renderer_link_shader_program(const shader_link_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_shader_program(u32 program_index) 
        {
        
        }
        
        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot) 
        {
            pool_grow(s_resource_pool, resource_slot);
            
            id<MTLBuffer> buf;
            
            u32 options = 0;
            if(params.cpu_access_flags & PEN_CPU_ACCESS_READ)
                options |= MTLResourceOptionCPUCacheModeDefault;
            else if(params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
                options |= MTLResourceCPUCacheModeWriteCombined;
            
            if(params.data)
            {
                buf = [s_metal_device newBufferWithBytes:params.data length:params.buffer_size options:options];
            }
            else
            {
                buf = [s_metal_device newBufferWithLength:params.buffer_size options:options];
            }
            
            s_resource_pool[resource_slot].buffer = { buf };
        }
        
        void renderer_set_vertex_buffers(u32* buffer_indices,
                                         u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets)
        {
            PEN_ASSERT(num_buffers < MAX_VB);
            
            vertex_buffer_cmd& vb = s_current_state.vertex_buffers;
            
            vb.num_buffers = num_buffers;
            vb.start_slot = start_slot;
            
            for(u32 i = 0; i < num_buffers; ++i)
            {
                vb.buffer_index[i] = buffer_indices[i];
                vb.stride[i] = strides[i];
                vb.offset[i] = offsets[i];
            }
        }
        
        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset) 
        {
        
        }
        
        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 shader_type) 
        {
        
        }
        
        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset) 
        {
        
        }
        
        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot) 
        {
        
        }
        
        void renderer_create_sampler(const sampler_creation_params& scp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags) 
        {
        
        }
        
        void renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_rasterizer_state(u32 rasterizer_state_index) 
        {
        
        }
        
        void renderer_set_viewport(const viewport& vp) 
        {
            s_current_state.viewport = vp;
        }
        
        void renderer_set_scissor_rect(const rect& r) 
        {
        
        }
        
        void renderer_create_blend_state(const blend_creation_params& bcp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_blend_state(u32 blend_state_index) 
        {
        
        }
        
        void renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_depth_stencil_state(u32 depth_stencil_state) 
        {
        
        }
        
        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology) 
        {
            bind_state();
            
            // draw calls
            [s_current_state.render_encoder drawPrimitives:MTLPrimitiveTypeTriangle
                                               vertexStart:start_vertex
                                               vertexCount:vertex_count];
            
            [s_current_state.render_encoder endEncoding];
            s_current_state.render_encoder = nil;
            
            // submit
            [s_current_state.cmd_buffer presentDrawable:s_current_state.drawable];
            [s_current_state.cmd_buffer commit];
        }
        
        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) 
        {
        
        }
        
        void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) 
        {
        
        }
        
        void renderer_draw_auto() 
        {
        
        }
        
        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
        
        }
        
        void renderer_set_targets(const u32* const colour_targets,
                                  u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face)
        {
        
        }
        
        void renderer_set_resolve_targets(u32 colour_target, u32 depth_target) 
        {
        
        }
        
        void renderer_set_stream_out_target(u32 buffer_index) 
        {
        
        }
        
        void renderer_resolve_target(u32 target, e_msaa_resolve_type type) 
        {
        
        }
        
        void renderer_read_back_resource(const resource_read_back_params& rrbp) 
        {
        
        }
        
        void renderer_present() 
        {
            
        }
        
        void renderer_push_perf_marker(const c8* name) 
        {
        
        }
        
        void renderer_pop_perf_marker() 
        {
        
        }
        
        void renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type) 
        {
        
        }
        
        void renderer_release_shader(u32 shader_index, u32 shader_type) 
        {
        
        }
        
        void renderer_release_program(u32 program) 
        {
        
        }
        
        void renderer_release_clear_state(u32 clear_state) 
        {
        
        }
        
        void renderer_release_buffer(u32 buffer_index) 
        {
        
        }
        
        void renderer_release_texture(u32 texture_index) 
        {
        
        }
        
        void renderer_release_sampler(u32 sampler) 
        {
        
        }
        
        void renderer_release_raster_state(u32 raster_state_index) 
        {
        
        }
        
        void renderer_release_blend_state(u32 blend_state) 
        {
        
        }
        
        void renderer_release_render_target(u32 render_target) 
        {
        
        }
        
        void renderer_release_input_layout(u32 input_layout) 
        {
        
        }
        
        void renderer_release_depth_stencil_state(u32 depth_stencil_state) 
        {
        
        }
    }
}

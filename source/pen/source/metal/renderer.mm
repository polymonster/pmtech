#import <MetalKit/MetalKit.h>
#import <Metal/Metal.h>

#include "renderer.h"
#include "console.h"
#include "data_struct.h"

// globals
a_u8 g_window_resize;

#define MAX_VB 4 // max simulataneously bound vertex buffers

namespace // structs and static vars
{
    struct clear_cmd
    {
        u32 clear_state;
        u32 colour_index;
        u32 depth_index;
    };
    
    struct vertex_buffer_cmd
    {
        id<MTLBuffer>   buffer[MAX_VB];
        u32             stride[MAX_VB];
        u32             offset[MAX_VB];
        u32             num_buffers;
        u32             start_slot;
    };
    
    struct index_buffer_cmd
    {
        id<MTLBuffer>   buffer;
        MTLIndexType    type;
        u32             offset;
    };
    
    struct current_state
    {
        // metal
        id<MTLRenderPipelineState>  pipeline;
        id<MTLCommandQueue>         command_queue;
        id<MTLRenderCommandEncoder> render_encoder;
        id<MTLCommandBuffer>        cmd_buffer;
        id<CAMetalDrawable>         drawable;
        
        // pen -> metal
        MTLViewport                 viewport;
        MTLVertexDescriptor*        vertex_descriptor;
        id<MTLFunction>             vertex_shader;
        id<MTLFunction>             fragment_shader;
        clear_cmd                   clear;
        vertex_buffer_cmd           vertex_buffers;
        index_buffer_cmd            index_buffer;
        u32                         input_layout;
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
    
    struct input_layout
    {
        u32 num_elements;
        pen::input_layout_desc slots[10];
    };
    
    struct resource
    {
        union
        {
            id<MTLBuffer>           buffer;
            shader_resource         shader;
            metal_clear_state       clear;
            MTLVertexDescriptor*    vertex_descriptor;
            input_layout            input_layout;
        };
    };
    
    resource*       _res_pool = nullptr;
    MTKView*        _metal_view;
    id<MTLDevice>   _metal_device;
    current_state   _state;
}

namespace // pen consts -> metal consts
{
    pen_inline MTLVertexFormat to_metal_vertex_format(u32 pen_vertex_format)
    {
        switch(pen_vertex_format)
        {
            case PEN_VERTEX_FORMAT_FLOAT1: return MTLVertexFormatFloat;
            case PEN_VERTEX_FORMAT_FLOAT2: return MTLVertexFormatFloat2;
            case PEN_VERTEX_FORMAT_FLOAT3: return MTLVertexFormatFloat3;
            case PEN_VERTEX_FORMAT_FLOAT4: return MTLVertexFormatFloat4;
            case PEN_VERTEX_FORMAT_UNORM4: return MTLVertexFormatUChar4Normalized;
            case PEN_VERTEX_FORMAT_UNORM2: return MTLVertexFormatUChar2Normalized;
            case PEN_VERTEX_FORMAT_UNORM1: return MTLVertexFormatUCharNormalized;
        }
        
        // unhandled
        PEN_ASSERT(0);
        return MTLVertexFormatInvalid;
    }
    
    pen_inline MTLIndexType to_metal_index_format(u32 pen_vertex_format)
    {
        return pen_vertex_format == PEN_FORMAT_R16_UINT ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
    }
    
    pen_inline u32 to_metal_pixel_format(u32 pen_vertex_format)
    {
        // unhandled
        PEN_ASSERT(0);
        return 0;
    }
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
            if(_state.render_encoder != 0)
                return;
            
            // rt and clear.. create render_encoder
            MTLRenderPassDescriptor *pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLCommandBuffer> cmd_buffer = [_state.command_queue commandBuffer];
            
            metal_clear_state& clear = _res_pool[_state.clear.clear_state].clear;
            
            id<CAMetalDrawable> drawable = _metal_view.currentDrawable;
            id<MTLTexture> texture = drawable.texture;
            
            pass_desc.colorAttachments[0].texture = texture; // todo rt
            pass_desc.colorAttachments[0].loadAction = clear.colour_load_action;
            pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass_desc.colorAttachments[0].clearColor = clear.colour[0];
            
            id <MTLRenderCommandEncoder> render_encoder = [cmd_buffer renderCommandEncoderWithDescriptor:pass_desc];
            
            // set viewport
            [render_encoder setViewport:_state.viewport];
            
            // create pipeline
            MTLRenderPipelineDescriptor *pipeline_desc = [MTLRenderPipelineDescriptor new];
            
            pipeline_desc.vertexFunction = _state.vertex_shader;
            pipeline_desc.fragmentFunction = _state.fragment_shader;
            pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            
            //pipeline_desc.vertexDescriptor = _state.vertex_descriptor;
            MTLVertexDescriptor* vd = [MTLVertexDescriptor vertexDescriptor];
            input_layout& il = _res_pool[_state.input_layout].input_layout;
            for(u32 i = 0; i < il.num_elements; ++i)
            {
                pen::input_layout_desc ild = il.slots[i];
                MTLVertexAttributeDescriptor *ad = [MTLVertexAttributeDescriptor new];
                ad.format = to_metal_vertex_format(ild.format);
                ad.offset = ild.aligned_byte_offset;
                ad.bufferIndex = ild.input_slot;

                [vd.attributes setObject: ad atIndexedSubscript: i];
            }

            MTLVertexBufferLayoutDescriptor *layout = [MTLVertexBufferLayoutDescriptor new];
            layout.stride = _state.vertex_buffers.stride[0];
            layout.stepFunction = MTLVertexStepFunctionPerVertex;
            layout.stepRate = 1;
            [vd.layouts setObject: layout atIndexedSubscript: 0];
        
            pipeline_desc.vertexDescriptor = vd;
            //_res_pool[resource_slot].vertex_descriptor = vd;
            
            NSError *error = nil;
            id<MTLRenderPipelineState> pipeline = [_metal_device newRenderPipelineStateWithDescriptor:pipeline_desc
                                                                                                 error:&error];
            
            [render_encoder setRenderPipelineState:pipeline];

            vertex_buffer_cmd& vb = _state.vertex_buffers;
            for(u32 i = 0; i < vb.num_buffers; ++i)
            {
                [render_encoder setVertexBuffer:vb.buffer[i]
                                         offset:vb.offset[i]
                                        atIndex:vb.start_slot+i];
            }
            
            _state.render_encoder = render_encoder;
            _state.cmd_buffer = cmd_buffer;
            _state.drawable = drawable;
        }
        
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res) 
        {
            _metal_view = (MTKView*)params;
            _metal_device = _metal_view.device;
            
            _state.command_queue = [_metal_device newCommandQueue];
            _state.render_encoder = 0;
            
            //reserve space for some resources
            sb_grow(_res_pool, 100);
            
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
            
            metal_clear_state& mc = _res_pool[resource_slot].clear;
            
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
            _state.clear = { clear_state_index, colour_face, depth_face };
        }
        
        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot) 
        {
            //pool_grow(s_resource_pool, resource_slot);
            
            const c8* csrc = (const c8*)params.byte_code;
            NSString* str = [[NSString alloc] initWithBytes:csrc length:params.byte_code_size
                                                   encoding:NSASCIIStringEncoding];
            
            NSError* err = nil;
            MTLCompileOptions* opts = [MTLCompileOptions alloc];
            id <MTLLibrary> lib = [_metal_device newLibraryWithSource:str options:opts error:&err];
            
            if(err)
                NSLog(@" error => %@ ", err);
            
            _res_pool[resource_slot].shader = { lib, params.type };
        }
        
        void renderer_set_shader(u32 shader_index, u32 shader_type) 
        {
            shader_resource& res = _res_pool[shader_index].shader;
            
            switch(shader_type)
            {
                case PEN_SHADER_TYPE_VS:
                    _state.vertex_shader = [res.lib newFunctionWithName:@"vs_main"];
                    break;
                case PEN_SHADER_TYPE_PS:
                    _state.fragment_shader = [res.lib newFunctionWithName:@"ps_main"];;
                    break;
            };
        }
        
        void renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot) 
        {
            MTLVertexDescriptor* vd = [MTLVertexDescriptor vertexDescriptor];
            
            for(u32 i = 0; i < params.num_elements; ++i)
            {
                input_layout_desc& il = params.input_layout[i];
                _res_pool[resource_slot].input_layout.slots[i] = il;
                
                /*
                MTLVertexAttributeDescriptor *ad = [MTLVertexAttributeDescriptor new];
                ad.format = to_metal_vertex_format(il.format);
                ad.offset = il.aligned_byte_offset;
                ad.bufferIndex = il.input_slot;
                
                [vd.attributes setObject: ad atIndexedSubscript: i];
                */
            }
            
            /*
            MTLVertexBufferLayoutDescriptor *layout = [MTLVertexBufferLayoutDescriptor new];
            layout.stride = 24;
            layout.stepFunction = MTLVertexStepFunctionPerVertex;
            layout.stepRate = 1;
            [vd.layouts setObject: layout atIndexedSubscript: 0];
            */
            
            //_res_pool[resource_slot].vertex_descriptor = vd;
        }
        
        void renderer_set_input_layout(u32 layout_index) 
        {
            //_state.vertex_descriptor = _res_pool[layout_index].vertex_descriptor;
            _state.input_layout = layout_index;
        }
        
        void renderer_link_shader_program(const shader_link_params& params, u32 resource_slot) 
        {
            // stub. used for opengl implementation
        }
        
        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot) 
        {
            pool_grow(_res_pool, resource_slot);
            
            id<MTLBuffer> buf;
            
            u32 options = 0;
            if(params.cpu_access_flags & PEN_CPU_ACCESS_READ)
                options |= MTLResourceOptionCPUCacheModeDefault;
            else if(params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
                options |= MTLResourceCPUCacheModeWriteCombined;
            
            if(params.data)
            {
                buf = [_metal_device newBufferWithBytes:params.data length:params.buffer_size options:options];
            }
            else
            {
                buf = [_metal_device newBufferWithLength:params.buffer_size options:options];
            }
            
            _res_pool[resource_slot].buffer = { buf };
        }
        
        void renderer_set_vertex_buffers(u32* buffer_indices,
                                         u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets)
        {
            PEN_ASSERT(num_buffers < MAX_VB);
            
            vertex_buffer_cmd& vb = _state.vertex_buffers;
            
            vb.num_buffers = num_buffers;
            vb.start_slot = start_slot;
            
            for(u32 i = 0; i < num_buffers; ++i)
            {
                u32 ri = buffer_indices[i];
                vb.buffer[i] = _res_pool[ri].buffer;
                vb.stride[i] = strides[i];
                vb.offset[i] = offsets[i];
            }
        }
        
        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset) 
        {
            index_buffer_cmd& ib = _state.index_buffer;
            ib.buffer = _res_pool[buffer_index].buffer;
            ib.type = MTLIndexTypeUInt16;
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
            _state.viewport = (MTLViewport){vp.x, vp.y, vp.width, vp.height, vp.min_depth, vp.max_depth};
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
            [_state.render_encoder drawPrimitives:MTLPrimitiveTypeTriangle
                                               vertexStart:start_vertex
                                               vertexCount:vertex_count];
            
            [_state.render_encoder endEncoding];
            _state.render_encoder = nil;
            
            // submit
            [_state.cmd_buffer presentDrawable:_state.drawable];
            [_state.cmd_buffer commit];
        }
        
        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) 
        {
            bind_state();
            
            // draw calls
            [_state.render_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                                       indexCount:index_count
                                                        indexType:_state.index_buffer.type
                                                      indexBuffer:_state.index_buffer.buffer
                                                indexBufferOffset:_state.index_buffer.offset];
            
            [_state.render_encoder endEncoding];
            _state.render_encoder = nil;
            
            [_state.cmd_buffer presentDrawable:_state.drawable];
            [_state.cmd_buffer commit];
        }
        
        void renderer_draw_indexed_instanced(u32 instance_count,
                                             u32 start_instance,
                                             u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
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

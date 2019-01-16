#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "console.h"
#include "data_struct.h"
#include "renderer.h"

// globals
a_u8 g_window_resize;

#define MAX_VB 4 // max simulataneously bound vertex buffers

using namespace pen;

namespace // structs and static vars
{
    struct clear_cmd
    {
        u32 clear_state;
        u32 colour_index;
        u32 depth_index;
    };

    struct index_buffer_cmd
    {
        id<MTLBuffer> buffer;
        MTLIndexType  type;
        u32           offset;
    };
    
    struct pixel_formats
    {
        MTLPixelFormat colour_attachments[pen::MAX_MRT];
    };

    struct current_state
    {
        // metal
        id<MTLRenderPipelineState>  pipeline;
        id<MTLCommandQueue>         command_queue;
        id<MTLRenderCommandEncoder> render_encoder;
        id<MTLCommandBuffer>        cmd_buffer;
        id<CAMetalDrawable>         drawable;
        MTLRenderPassDescriptor*    pass;
        pixel_formats               formats;

        // pen -> metal
        MTLViewport          viewport;
        MTLVertexDescriptor* vertex_descriptor;
        id<MTLFunction>      vertex_shader;
        id<MTLFunction>      fragment_shader;
        index_buffer_cmd     index_buffer;
        u32                  input_layout;
    };

    struct shader_resource
    {
        id<MTLLibrary> lib;
        u32            type;
    };
    
    struct texture_resource
    {
        id<MTLTexture> tex;
        MTLPixelFormat fmt;
    };

    struct metal_clear_state
    {
        MTLLoadAction colour_load_action;
        MTLClearColor colour[pen::MAX_MRT];
    };

    struct resource
    {
        union {
            id<MTLBuffer>        buffer;
            texture_resource     texture;
            id<MTLSamplerState>  sampler;
            shader_resource      shader;
            metal_clear_state    clear;
            MTLVertexDescriptor* vertex_descriptor;
        };
    };

    res_pool<resource>  _res_pool;
    MTKView*            _metal_view;
    id<MTLDevice>       _metal_device;
    current_state       _state;
}

namespace // pen consts -> metal consts
{
    pen_inline MTLVertexFormat to_metal_vertex_format(u32 pen_vertex_format)
    {
        switch (pen_vertex_format)
        {
            case PEN_VERTEX_FORMAT_FLOAT1:
                return MTLVertexFormatFloat;
            case PEN_VERTEX_FORMAT_FLOAT2:
                return MTLVertexFormatFloat2;
            case PEN_VERTEX_FORMAT_FLOAT3:
                return MTLVertexFormatFloat3;
            case PEN_VERTEX_FORMAT_FLOAT4:
                return MTLVertexFormatFloat4;
            case PEN_VERTEX_FORMAT_UNORM4:
                return MTLVertexFormatUChar4Normalized;
            case PEN_VERTEX_FORMAT_UNORM2:
                return MTLVertexFormatUChar2Normalized;
            case PEN_VERTEX_FORMAT_UNORM1:
                return MTLVertexFormatUCharNormalized;
        }

        // unhandled
        PEN_ASSERT(0);
        return MTLVertexFormatInvalid;
    }

    pen_inline MTLIndexType to_metal_index_format(u32 pen_vertex_format)
    {
        return pen_vertex_format == PEN_FORMAT_R16_UINT ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
    }
    
    pen_inline MTLPixelFormat to_metal_pixel_format(u32 pen_vertex_format)
    {
        switch (pen_vertex_format)
        {
            case PEN_TEX_FORMAT_RGBA8_UNORM:
                return MTLPixelFormatRGBA8Unorm;
            case PEN_TEX_FORMAT_BGRA8_UNORM:
                return MTLPixelFormatBGRA8Unorm;
            case PEN_TEX_FORMAT_R32G32B32A32_FLOAT:
                return MTLPixelFormatRGBA32Float;
            case PEN_TEX_FORMAT_R32_FLOAT:
                return MTLPixelFormatR32Float;
            case PEN_TEX_FORMAT_R16G16B16A16_FLOAT:
                return MTLPixelFormatRGBA16Float;
            case PEN_TEX_FORMAT_R16_FLOAT:
                return MTLPixelFormatR16Float;
            case PEN_TEX_FORMAT_R32_UINT:
                return MTLPixelFormatR32Uint;
            case PEN_TEX_FORMAT_R8_UNORM:
                return MTLPixelFormatR8Unorm;
            case PEN_TEX_FORMAT_BC1_UNORM:
                return MTLPixelFormatBC1_RGBA;
            case PEN_TEX_FORMAT_BC2_UNORM:
                return MTLPixelFormatBC2_RGBA;
            case PEN_TEX_FORMAT_BC3_UNORM:
                return MTLPixelFormatBC3_RGBA;
            case PEN_TEX_FORMAT_BC4_UNORM:
                return MTLPixelFormatBC4_RUnorm;
            case PEN_TEX_FORMAT_BC5_UNORM:
                return MTLPixelFormatBC5_RGUnorm;
            case PEN_TEX_FORMAT_D24_UNORM_S8_UINT:
                return MTLPixelFormatDepth24Unorm_Stencil8;
                break;
        }
        
        // unhandled
        PEN_ASSERT(0);
        return MTLPixelFormatInvalid;
    }
    
    pen_inline MTLTextureUsage to_metal_texture_usage(u32 bind_flags)
    {
        MTLTextureUsage usage = 0;
        if(bind_flags & PEN_BIND_SHADER_RESOURCE)
            usage |= MTLTextureUsageShaderRead;
            
        if(bind_flags & PEN_BIND_RENDER_TARGET)
            usage |= MTLTextureUsageRenderTarget;
        
        return usage;
    }
    
    pen_inline MTLSamplerAddressMode to_metal_sampler_address_mode(u32 address_mode)
    {
        switch (address_mode)
        {
            case PEN_TEXTURE_ADDRESS_WRAP:
                return MTLSamplerAddressModeRepeat;
            case PEN_TEXTURE_ADDRESS_MIRROR:
                return MTLSamplerAddressModeMirrorRepeat;
            case PEN_TEXTURE_ADDRESS_CLAMP:
                return MTLSamplerAddressModeClampToEdge;
            case PEN_TEXTURE_ADDRESS_MIRROR_ONCE:
                return MTLSamplerAddressModeMirrorClampToEdge;
        }
        
        // unhandled
        PEN_ASSERT(0);
        return MTLSamplerAddressModeRepeat;
    }
    
    pen_inline MTLSamplerMinMagFilter to_metal_min_mag_filter(u32 filter)
    {
        switch (filter)
        {
            case PEN_FILTER_MIN_MAG_MIP_LINEAR:
            case PEN_FILTER_LINEAR:
                return MTLSamplerMinMagFilterLinear;
            case PEN_FILTER_MIN_MAG_MIP_POINT:
            case PEN_FILTER_POINT:
                return MTLSamplerMinMagFilterNearest;
        }
        
        // unhandled
        PEN_ASSERT(0);
        return MTLSamplerMinMagFilterLinear;
    }
    
    pen_inline MTLSamplerMipFilter to_metal_mip_filter(u32 filter)
    {
        switch (filter)
        {
            case PEN_FILTER_MIN_MAG_MIP_LINEAR:
                return MTLSamplerMipFilterLinear;
            case PEN_FILTER_MIN_MAG_MIP_POINT:
                return MTLSamplerMipFilterNearest;
            case PEN_FILTER_LINEAR:
            case PEN_FILTER_POINT:
                return MTLSamplerMipFilterNotMipmapped;
        }
        
        // unhandled
        PEN_ASSERT(0);
        return MTLSamplerMipFilterNotMipmapped;
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
        void validate_encoder()
        {
            if(!_state.render_encoder)
            {
                _state.render_encoder = [_state.cmd_buffer renderCommandEncoderWithDescriptor:_state.pass];
                [_state.render_encoder setViewport:_state.viewport];
            }
        }
        
        void bind_pipeline()
        {
            // create pipeline
            MTLRenderPipelineDescriptor* pipeline_desc = [MTLRenderPipelineDescriptor new];

            pipeline_desc.vertexFunction = _state.vertex_shader;
            pipeline_desc.fragmentFunction = _state.fragment_shader;
            pipeline_desc.colorAttachments[0].pixelFormat = _state.formats.colour_attachments[0];

            pipeline_desc.vertexDescriptor = _state.vertex_descriptor;

            NSError*                   error = nil;
            id<MTLRenderPipelineState> pipeline =
                [_metal_device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];

            [_state.render_encoder setRenderPipelineState:pipeline];
        }
        
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res)
        {
            _metal_view = (MTKView*)params;
            _metal_device = _metal_view.device;

            _state.command_queue = [_metal_device newCommandQueue];
            _state.render_encoder = 0;

            //reserve space for some resources
            _res_pool.init(1);

            return 1;
        }

        void renderer_shutdown()
        {
            // nothing to do
        }

        void renderer_make_context_current()
        {
            // stub. used for gl implementation
        }

        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot)
        {
            _res_pool.insert(resource(), resource_slot);
            metal_clear_state& mc = _res_pool.get(resource_slot).clear;

            // flags
            mc.colour_load_action = MTLLoadActionLoad;
            if (cs.flags & PEN_CLEAR_COLOUR_BUFFER)
                mc.colour_load_action = MTLLoadActionClear;

            // colour
            mc.colour[0] = MTLClearColorMake(cs.r, cs.g, cs.b, cs.a);

            for (u32 i = 0; i < cs.num_colour_targets; ++i)
            {
                const mrt_clear& m = cs.mrt[i];

                switch (m.type)
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
            metal_clear_state& clear = _res_pool.get(clear_state_index).clear;
            
            _state.pass.colorAttachments[0].loadAction = clear.colour_load_action;
            _state.pass.colorAttachments[0].clearColor = clear.colour[0];
        }

        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot)
        {
            const c8* csrc = (const c8*)params.byte_code;
            NSString* str = [[NSString alloc] initWithBytes:csrc length:params.byte_code_size encoding:NSASCIIStringEncoding];

            NSError*           err = nil;
            MTLCompileOptions* opts = [MTLCompileOptions alloc];
            id<MTLLibrary>     lib = [_metal_device newLibraryWithSource:str options:opts error:&err];

            if (err)
                NSLog(@" error => %@ ", err);

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).shader = {lib, params.type};
        }

        void renderer_set_shader(u32 shader_index, u32 shader_type)
        {
            shader_resource& res = _res_pool.get(shader_index).shader;

            switch (shader_type)
            {
                case PEN_SHADER_TYPE_VS:
                    _state.vertex_shader = [res.lib newFunctionWithName:@"vs_main"];
                    break;
                case PEN_SHADER_TYPE_PS:
                    _state.fragment_shader = [res.lib newFunctionWithName:@"ps_main"];
                    break;
            };
        }

        void renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot)
        {
            MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];

            for (u32 i = 0; i < params.num_elements; ++i)
            {
                input_layout_desc& il = params.input_layout[i];

                MTLVertexAttributeDescriptor* ad = [MTLVertexAttributeDescriptor new];
                ad.format = to_metal_vertex_format(il.format);
                ad.offset = il.aligned_byte_offset;
                ad.bufferIndex = il.input_slot;

                [vd.attributes setObject:ad atIndexedSubscript:i];
            }

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).vertex_descriptor = vd;
        }

        void renderer_set_input_layout(u32 layout_index)
        {
            _state.vertex_descriptor = _res_pool.get(layout_index).vertex_descriptor;
            _state.input_layout = layout_index;
        }

        void renderer_link_shader_program(const shader_link_params& params, u32 resource_slot)
        {
            // stub. used for opengl implementation
        }

        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot)
        {
            id<MTLBuffer> buf;

            u32 options = 0;
            if (params.cpu_access_flags & PEN_CPU_ACCESS_READ)
                options |= MTLResourceOptionCPUCacheModeDefault;
            else if (params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
                options |= MTLResourceCPUCacheModeWriteCombined;

            if (params.data)
            {
                buf = [_metal_device newBufferWithBytes:params.data length:params.buffer_size options:options];
            }
            else
            {
                buf = [_metal_device newBufferWithLength:params.buffer_size options:options];
            }

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).buffer = {buf};
        }

        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                         const u32* offsets)
        {
            validate_encoder();

            for (u32 i = 0; i < num_buffers; ++i)
            {
                u32 ri = buffer_indices[i];

                [_state.render_encoder setVertexBuffer:_res_pool.get(ri).buffer offset:offsets[i] atIndex:start_slot + i];

                MTLVertexBufferLayoutDescriptor* layout = [MTLVertexBufferLayoutDescriptor new];
                layout.stride = strides[i];
                layout.stepFunction = MTLVertexStepFunctionPerVertex;
                layout.stepRate = 1;
                [_state.vertex_descriptor.layouts setObject:layout atIndexedSubscript:start_slot + i];
            }
        }

        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
        {
            index_buffer_cmd& ib = _state.index_buffer;
            ib.buffer = _res_pool.get(buffer_index).buffer;
            ib.type = to_metal_index_format(format);
        }

        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 shader_type)
        {
            
        }

        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
        {
            
        }

        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
        {
            MTLTextureDescriptor* td = nil;
            id<MTLTexture>        texture = nil;
            
            MTLPixelFormat fmt = to_metal_pixel_format(tcp.format);

            if (tcp.collection_type == TEXTURE_COLLECTION_NONE)
            {
                td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                        width:tcp.width
                                                                       height:tcp.height
                                                                    mipmapped:tcp.num_mips > 1];

                td.usage = to_metal_texture_usage(tcp.bind_flags);
                
                texture = [_metal_device newTextureWithDescriptor:td];

                if (tcp.data)
                {
                    u32 mip_w = tcp.width;
                    u32 mip_h = tcp.height;
                    u8* mip_data = (u8*)tcp.data;
                    
                    for(u32 i = 0; i < tcp.num_mips; ++i)
                    {
                        u32       pitch = tcp.block_size * tcp.pixels_per_block * mip_w;
                        MTLRegion region = MTLRegionMake2D(0, 0, mip_w, mip_h);
                        
                        [texture replaceRegion:region mipmapLevel:i withBytes:mip_data bytesPerRow:pitch];
                        
                        mip_data += pitch * mip_h;
                        
                        // images may be non-pot
                        mip_w /= 2;
                        mip_h /= 2;
                    }
                }
            }
            else
            {
                // todo cube, 3d volume, arrays etc
            }

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).texture = { texture, fmt };
        }

        void renderer_create_sampler(const sampler_creation_params& scp, u32 resource_slot)
        {
            // create sampler state
            MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
            
            sd.sAddressMode = to_metal_sampler_address_mode(scp.address_u);
            sd.tAddressMode = to_metal_sampler_address_mode(scp.address_v);
            sd.rAddressMode = to_metal_sampler_address_mode(scp.address_w);
            
            sd.minFilter = to_metal_min_mag_filter(scp.filter);
            sd.magFilter = to_metal_min_mag_filter(scp.filter);
            sd.mipFilter = to_metal_mip_filter(scp.filter);

            sd.lodMinClamp = scp.min_lod;
            sd.lodMaxClamp = scp.max_lod;
            sd.maxAnisotropy = 1.0f + scp.max_anisotropy;
            
            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).sampler = [_metal_device newSamplerStateWithDescriptor:sd];
        }

        void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags)
        {
            validate_encoder();
            
            if(texture_index == 0)
                return;
            
            PEN_ASSERT(_state.render_encoder);

            if (bind_flags & pen::TEXTURE_BIND_PS)
            {
                [_state.render_encoder setFragmentTexture:_res_pool.get(texture_index).texture.tex atIndex:resource_slot];
                [_state.render_encoder setFragmentSamplerState:_res_pool.get(sampler_index).sampler atIndex:resource_slot];
            }

            if (bind_flags & pen::TEXTURE_BIND_VS)
            {
                [_state.render_encoder setVertexTexture:_res_pool.get(texture_index).texture.tex atIndex:resource_slot];
                [_state.render_encoder setVertexSamplerState:_res_pool.get(sampler_index).sampler atIndex:resource_slot];
            }
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
            validate_encoder();
            bind_pipeline();

            // draw calls
            [_state.render_encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:start_vertex vertexCount:vertex_count];
        }

        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
        {
            validate_encoder();
            bind_pipeline();

            // draw calls
            [_state.render_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                              indexCount:index_count
                                               indexType:_state.index_buffer.type
                                             indexBuffer:_state.index_buffer.buffer
                                       indexBufferOffset:_state.index_buffer.offset];
        }

        void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                             u32 base_vertex, u32 primitive_topology)
        {
        }

        void renderer_draw_auto()
        {
        }

        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
            renderer_create_texture(tcp, resource_slot);
        }

        void renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face,
                                  u32 depth_face)
        {
            // create new cmd buffer
            if (_state.cmd_buffer == nil)
                _state.cmd_buffer = [_state.command_queue commandBuffer];
            
            // finish render encoding
            if(_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
            }
            
            _state.pass = [MTLRenderPassDescriptor renderPassDescriptor];
            
            if(num_colour_targets == 1 && colour_targets[0] == PEN_BACK_BUFFER_COLOUR)
            {
                // backbuffer colour target
                _state.drawable = _metal_view.currentDrawable;
                id<MTLTexture> texture = _state.drawable.texture;
                
                _state.pass.colorAttachments[0].texture = texture;
                _state.pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                _state.formats.colour_attachments[0] = _metal_view.colorPixelFormat;
            }
            else
            {
                // render target
                for(u32 i = 0; i < num_colour_targets; ++i)
                {
                    // render target
                    id<MTLTexture> texture = _res_pool.get(colour_targets[i]).texture.tex;
                    
                    _state.pass.colorAttachments[i].texture = texture;
                    _state.pass.colorAttachments[i].loadAction = MTLLoadActionDontCare;
                    _state.pass.colorAttachments[i].storeAction = MTLStoreActionStore;
                    
                    _state.formats.colour_attachments[i] = _res_pool.get(colour_targets[i]).texture.fmt;
                }
            }
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
            if(_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
            }
            
            // flush cmd buf and present
            [_state.cmd_buffer presentDrawable:_state.drawable];
            [_state.cmd_buffer commit];
            
            // null state for next frame
            _state.cmd_buffer = nil;
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
            _res_pool.get(shader_index).shader.lib = nil;
        }

        void renderer_release_clear_state(u32 clear_state)
        {
        }

        void renderer_release_buffer(u32 buffer_index)
        {
            _res_pool.get(buffer_index).buffer = nil;
        }

        void renderer_release_texture(u32 texture_index)
        {
            _res_pool.get(texture_index).texture.tex = nil;
        }

        void renderer_release_sampler(u32 sampler)
        {
            _res_pool.get(sampler).sampler = nil;
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
            _res_pool.get(input_layout).vertex_descriptor = nil;
        }

        void renderer_release_depth_stencil_state(u32 depth_stencil_state)
        {
            
        }
    }
}

// renderer.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "console.h"
#include "data_struct.h"
#include "renderer.h"
#include "str/Str.h"
#include "threads.h"
#include "hash.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

using namespace pen;

// globals / externs
a_u8 g_window_resize;
extern a_u64 g_frame_index;
extern window_creation_params pen_window;

namespace // internal structs and static vars
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
        u32           size_bytes;
    };

    struct pixel_formats
    {
        MTLPixelFormat colour_attachments[pen::MAX_MRT];
        MTLPixelFormat depth_attachment;
        u32            sample_count;
    };

    struct current_state
    {
        // metal
        id<MTLRenderPipelineState>  pipeline;
        id<MTLCommandQueue>         command_queue;
        id<MTLRenderCommandEncoder> render_encoder;
        id<MTLCommandBuffer>        cmd_buffer;
        id<MTLDepthStencilState>    depth_stencil;
        id<CAMetalDrawable>         drawable;
        MTLRenderPassDescriptor*    pass;

        // pen -> metal
        MTLViewport          viewport;
        MTLScissorRect       scissor;
        index_buffer_cmd     index_buffer;
        u32                  input_layout;
        
        // hashable to rebuild pipe
        pixel_formats        formats;
        MTLVertexDescriptor* vertex_descriptor;
        id<MTLFunction>      vertex_shader;
        id<MTLFunction>      fragment_shader;
        u32                  blend_state;
        
        // cache
        hash_id              pipeline_hash;
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
        u32           num_colour_targets;
        MTLClearColor colour[pen::MAX_MRT];
        MTLLoadAction colour_load_action[pen::MAX_MRT];
        f32           depth_clear;
        MTLLoadAction depth_load_action;
    };

    struct metal_target_blend
    {
        bool              enabled;
        MTLBlendOperation rgb_op;
        MTLBlendOperation alpha_op;
        MTLBlendFactor    src_rgb_factor;
        MTLBlendFactor    dst_rgb_factor;
        MTLBlendFactor    src_alpha_factor;
        MTLBlendFactor    dst_alpha_factor;
        MTLColorWriteMask write_mask;
    };

    struct metal_blend_state
    {
        bool               alpha_to_coverage_enable;
        u32                num_render_targets;
        metal_target_blend attachment[pen::MAX_MRT];
    };
    
    struct resource
    {
        union {
            pen::multi_buffer<id<MTLBuffer>, 2> buffer;
            texture_resource                    texture;
            id<MTLSamplerState>                 sampler;
            shader_resource                     shader;
            metal_clear_state                   clear;
            MTLVertexDescriptor*                vertex_descriptor;
            metal_blend_state                   blend;
            id<MTLDepthStencilState>            depth_stencil;
        };
        
        resource() {};
    };
    
    struct managed_rt
    {
        pen::texture_creation_params tcp;
        u32                          rt;
    };

    res_pool<resource> _res_pool;
    MTKView*           _metal_view;
    id<MTLDevice>      _metal_device;
    current_state      _state;
    a_u64              _frame_sync;
    managed_rt*        _managed_rts = nullptr;
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

    pen_inline u32 index_size_bytes(u32 pen_vertex_format)
    {
        if (pen_vertex_format == PEN_FORMAT_R16_UINT)
            return 2;

        return 4;
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
        if (bind_flags & PEN_BIND_SHADER_RESOURCE)
            usage |= MTLTextureUsageShaderRead;

        if (bind_flags & PEN_BIND_RENDER_TARGET)
            usage |= MTLTextureUsageRenderTarget;
        
        if(bind_flags & PEN_BIND_DEPTH_STENCIL)
            usage |= MTLTextureUsageRenderTarget;

        return usage;
    }
    
    pen_inline MTLStorageMode to_metal_storage_mode(const texture_creation_params& tcp)
    {
        if(tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT || tcp.sample_count > 1)
            return MTLStorageModePrivate;

        return MTLStorageModeManaged;
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

    pen_inline const char* get_metal_version_string()
    {
        if ([_metal_device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v2])
        {
            return "Metal MacOS 2.0";
        }
        else if ([_metal_device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v1])
        {
            return "Metal MacOS 1.0";
        }

        return "";
    }

    pen_inline MTLPrimitiveType to_metal_primitive_type(u32 pt)
    {
        switch (pt)
        {
            case PEN_PT_POINTLIST:
                return MTLPrimitiveTypePoint;
            case PEN_PT_LINELIST:
                return MTLPrimitiveTypeLine;
            case PEN_PT_LINESTRIP:
                return MTLPrimitiveTypeLineStrip;
            case PEN_PT_TRIANGLELIST:
                return MTLPrimitiveTypeTriangle;
            case PEN_PT_TRIANGLESTRIP:
                return MTLPrimitiveTypeTriangleStrip;
        };

        PEN_ASSERT(0);
        return MTLPrimitiveTypeTriangle;
    }

    pen_inline MTLBlendOperation to_metal_blend_op(u32 bo)
    {
        switch (bo)
        {
            case PEN_BLEND_OP_ADD:
                return MTLBlendOperationAdd;
            case PEN_BLEND_OP_SUBTRACT:
                return MTLBlendOperationSubtract;
            case PEN_BLEND_OP_REV_SUBTRACT:
                return MTLBlendOperationReverseSubtract;
            case PEN_BLEND_OP_MIN:
                return MTLBlendOperationMin;
            case PEN_BLEND_OP_MAX:
                return MTLBlendOperationMax;
        };

        PEN_ASSERT(0);
        return MTLBlendOperationAdd;
    }

    pen_inline MTLBlendFactor to_metal_blend_factor(u32 bf)
    {
        switch (bf)
        {
            case PEN_BLEND_ZERO:
                return MTLBlendFactorZero;
            case PEN_BLEND_ONE:
                return MTLBlendFactorOne;
            case PEN_BLEND_SRC_COLOR:
                return MTLBlendFactorSourceColor;
            case PEN_BLEND_INV_SRC_COLOR:
                return MTLBlendFactorOneMinusSourceColor;
            case PEN_BLEND_SRC_ALPHA:
                return MTLBlendFactorSourceAlpha;
            case PEN_BLEND_INV_SRC_ALPHA:
                return MTLBlendFactorOneMinusSourceAlpha;
            case PEN_BLEND_DEST_ALPHA:
                return MTLBlendFactorDestinationAlpha;
            case PEN_BLEND_INV_DEST_ALPHA:
                return MTLBlendFactorOneMinusDestinationAlpha;
            case PEN_BLEND_DEST_COLOR:
                return MTLBlendFactorDestinationColor;
            case PEN_BLEND_INV_DEST_COLOR:
                return MTLBlendFactorOneMinusDestinationColor;
            case PEN_BLEND_SRC_ALPHA_SAT:
                return MTLBlendFactorSourceAlphaSaturated;
            case PEN_BLEND_SRC1_COLOR:
                return MTLBlendFactorSource1Color;
            case PEN_BLEND_INV_SRC1_COLOR:
                return MTLBlendFactorOneMinusSource1Color;
            case PEN_BLEND_SRC1_ALPHA:
                return MTLBlendFactorSource1Alpha;
            case PEN_BLEND_INV_SRC1_ALPHA:
                return MTLBlendFactorOneMinusSource1Alpha;
            case PEN_BLEND_BLEND_FACTOR:
                return MTLBlendFactorBlendAlpha;
            case PEN_BLEND_INV_BLEND_FACTOR:
                return MTLBlendFactorOneMinusBlendAlpha;
        };

        PEN_ASSERT(0);
        return MTLBlendFactorZero;
    }
    
    pen_inline MTLCompareFunction to_metal_compare_function(u32 cf)
    {
        switch(cf)
        {
            case PEN_COMPARISON_NEVER:
                return MTLCompareFunctionNever;
            case PEN_COMPARISON_LESS:
                return MTLCompareFunctionLess;
            case PEN_COMPARISON_EQUAL:
                return MTLCompareFunctionEqual;
            case PEN_COMPARISON_LESS_EQUAL:
                return MTLCompareFunctionLessEqual;
            case PEN_COMPARISON_GREATER:
                return MTLCompareFunctionGreater;
            case PEN_COMPARISON_NOT_EQUAL:
                return MTLCompareFunctionNotEqual;
            case PEN_COMPARISON_GREATER_EQUAL:
                return MTLCompareFunctionGreaterEqual;
            case PEN_COMPARISON_ALWAYS:
                return MTLCompareFunctionAlways;
        }
        
        PEN_ASSERT(0);
        return MTLCompareFunctionAlways;
    }
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

        const Str device_name = (const char*)[_metal_device.name UTF8String];
        const Str version_name = get_metal_version_string();

        info.api_version = version_name.c_str();
        info.shader_version = "metal";
        info.renderer_cmd = "metal";
        info.renderer = device_name.c_str();
        info.vendor = device_name.c_str();

        return info;
    }

    namespace direct
    {
        void validate_encoder()
        {
            if (!_state.render_encoder)
            {
                _state.render_encoder = [_state.cmd_buffer renderCommandEncoderWithDescriptor:_state.pass];
            }

            [_state.render_encoder setViewport:_state.viewport];
            [_state.render_encoder setScissorRect:_state.scissor];
            
            if(_state.depth_stencil && _state.formats.depth_attachment != MTLPixelFormatInvalid)
                [_state.render_encoder setDepthStencilState:_state.depth_stencil];
        }

        void bind_pipeline()
        {
            // pipeline hash
            HashMurmur2A hh;
            hh.begin();
            static const size_t off = (u8*)&_state.pipeline_hash - (u8*)&_state.formats;
            hh.add(&_state.formats, off);
            
            hash_id cur = hh.end();
            if(cur == _state.pipeline_hash)
                return;
            
            _state.pipeline_hash = cur;

            // create pipeline
            MTLRenderPipelineDescriptor* pipeline_desc = [MTLRenderPipelineDescriptor new];

            pipeline_desc.vertexFunction = _state.vertex_shader;
            pipeline_desc.fragmentFunction = _state.fragment_shader;
            pipeline_desc.sampleCount = _state.formats.sample_count;
            pipeline_desc.colorAttachments[0].pixelFormat = _state.formats.colour_attachments[0];
            pipeline_desc.depthAttachmentPixelFormat = _state.formats.depth_attachment;

            // apply blend state
            metal_blend_state& blend = _res_pool.get(_state.blend_state).blend;
            for (u32 i = 0; i < blend.num_render_targets; ++i)
            {
                MTLRenderPipelineColorAttachmentDescriptor* ca = pipeline_desc.colorAttachments[i];
                metal_target_blend&                         tb = blend.attachment[i];

                ca.blendingEnabled = tb.enabled;
                ca.rgbBlendOperation = tb.rgb_op;
                ca.alphaBlendOperation = tb.alpha_op;
                ca.sourceRGBBlendFactor = tb.src_rgb_factor;
                ca.destinationRGBBlendFactor = tb.dst_rgb_factor;
                ca.sourceAlphaBlendFactor = tb.src_rgb_factor;
                ca.destinationAlphaBlendFactor = tb.dst_alpha_factor;
                ca.writeMask = tb.write_mask;
            }

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
            _frame_sync = 0;

            return 1;
        }

        void renderer_shutdown()
        {
            // nothing to do yet
        }

        void renderer_make_context_current()
        {
            _frame_sync = g_frame_index.load();
        }
        
        void renderer_sync()
        {
            while(_frame_sync == g_frame_index.load())
                thread_sleep_us(100);
        }

        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot)
        {
            _res_pool.insert(resource(), resource_slot);
            metal_clear_state& mc = _res_pool.get(resource_slot).clear;
            
            mc.num_colour_targets = cs.num_colour_targets;

            // flags
            mc.colour_load_action[0] = MTLLoadActionLoad;
            if (cs.flags & PEN_CLEAR_COLOUR_BUFFER)
            {
                mc.colour_load_action[0] = MTLLoadActionClear;
                
                // single
                if(mc.num_colour_targets == 0)
                    mc.num_colour_targets = 1;
            }
            
            // colour
            mc.colour[0] = MTLClearColorMake(cs.r, cs.g, cs.b, cs.a);

            // mrt
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
            mc.depth_load_action = MTLLoadActionLoad;
            if (cs.flags & PEN_CLEAR_DEPTH_BUFFER)
                mc.depth_load_action = MTLLoadActionClear;
            
            mc.depth_clear = cs.depth;
        }

        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {
            metal_clear_state& clear = _res_pool.get(clear_state_index).clear;
            
            for(u32 c = 0; c < clear.num_colour_targets; ++c)
            {
                _state.pass.colorAttachments[c].loadAction = clear.colour_load_action[c];
                _state.pass.colorAttachments[c].clearColor = clear.colour[c];
            }
            
            if(_state.pass.depthAttachment)
            {
                _state.pass.depthAttachment.loadAction = clear.depth_load_action;
                _state.pass.depthAttachment.clearDepth = clear.depth_clear;
            }

            validate_encoder();
        }

        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot)
        {
            const c8* csrc = (const c8*)params.byte_code;
            NSString* str = [[NSString alloc] initWithBytes:csrc length:params.byte_code_size encoding:NSASCIIStringEncoding];

            NSError*           err = nil;
            MTLCompileOptions* opts = [MTLCompileOptions alloc];
            id<MTLLibrary>     lib = [_metal_device newLibraryWithSource:str options:opts error:&err];

            if (err)
            {
                if (err.code == 3)
                {
                    NSLog(@" error => %@ ", err);
                }
            }
            
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
                    PEN_ASSERT(_state.vertex_shader);
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
            id<MTLBuffer> buf[2];

            u32 options = 0;
            u32 num_bufs = 1;
            
            if (params.cpu_access_flags & PEN_CPU_ACCESS_READ)
            {
                options |= MTLResourceOptionCPUCacheModeDefault;
            }
            else if (params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
            {
                options |= MTLResourceCPUCacheModeWriteCombined;
                num_bufs = 2;
            }

            for(u32 i = 0; i < num_bufs; ++i)
            {
                if (params.data)
                {
                    buf[i] = [_metal_device newBufferWithBytes:params.data length:params.buffer_size options:options];
                }
                else
                {
                    buf[i] = [_metal_device newBufferWithLength:params.buffer_size options:options];
                }
            }

            _res_pool.insert(resource(), resource_slot);
            
            _res_pool.get(resource_slot).buffer._fb = 0;
            _res_pool.get(resource_slot).buffer._bb = 1;
            
            for(u32 i = 0; i < num_bufs; ++i)
                _res_pool.get(resource_slot).buffer._data[i] = {buf[i]};
        }

        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                         const u32* offsets)
        {
            validate_encoder();

            for (u32 i = 0; i < num_buffers; ++i)
            {
                u32 ri = buffer_indices[i];
                u32 stride = strides[i];

                [_state.render_encoder setVertexBuffer:_res_pool.get(ri).buffer.frontbuffer()
                                                offset:offsets[i] atIndex:start_slot + i];

                MTLVertexBufferLayoutDescriptor* layout = [MTLVertexBufferLayoutDescriptor new];
                
                layout.stride = stride;
                layout.stepFunction = i == 0 ? MTLVertexStepFunctionPerVertex : MTLVertexStepFunctionPerInstance;
                layout.stepRate = 1;
                
                [_state.vertex_descriptor.layouts setObject:layout atIndexedSubscript:start_slot + i];
            }
        }

        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
        {
            index_buffer_cmd& ib = _state.index_buffer;
            ib.buffer = _res_pool.get(buffer_index).buffer.frontbuffer();
            ib.type = to_metal_index_format(format);
            ib.offset = offset;
            ib.size_bytes = index_size_bytes(format);
        }

        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            validate_encoder();

            u32 bi = buffer_index;
            
            if(flags & pen::CBUFFER_BIND_VS)
                [_state.render_encoder setVertexBuffer:_res_pool.get(bi).buffer.frontbuffer()
                                                offset:0 atIndex:resource_slot + 8];
            
            if(flags & pen::CBUFFER_BIND_PS)
                [_state.render_encoder setFragmentBuffer:_res_pool.get(bi).buffer.frontbuffer()
                                                  offset:0 atIndex:resource_slot + 8];
        }

        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
        {
            resource& r = _res_pool.get(buffer_index);
            id<MTLBuffer>& bb = r.buffer.backbuffer();
            
            u8* pdata = (u8*)[bb contents];
            pdata = pdata + offset;

            memcpy(pdata, data, data_size);
            
            r.buffer.swap_buffers();
        }

        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
        {
            texture_creation_params _tcp = tcp;
            if(tcp.width == PEN_INVALID_HANDLE)
            {
                _tcp.width = pen_window.width;
                _tcp.height = pen_window.height;
                
                // todo track rt
            }
            
            MTLTextureDescriptor* td = nil;
            id<MTLTexture>        texture = nil;

            MTLPixelFormat fmt = to_metal_pixel_format(tcp.format);

            u32 num_slices = 1;
            u32 num_arrays = 1;
            
            if (tcp.collection_type == TEXTURE_COLLECTION_NONE ||
                tcp.collection_type == TEXTURE_COLLECTION_ARRAY)
            {
                td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                        width:_tcp.width
                                                                       height:_tcp.height
                                                                    mipmapped:_tcp.num_mips > 1 ];
                
                if(tcp.collection_type == TEXTURE_COLLECTION_ARRAY)
                {
                    td.textureType = MTLTextureType2DArray;
                    num_arrays = _tcp.num_arrays;
                }
            }
            else if(tcp.collection_type == TEXTURE_COLLECTION_CUBE)
            {
                td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:fmt
                                                                           size:_tcp.width
                                                                      mipmapped:_tcp.num_mips > 1];
                
                num_arrays = _tcp.num_arrays;
            }
            else if(tcp.collection_type == TEXTURE_COLLECTION_VOLUME)
            {
                td = [MTLTextureDescriptor alloc];
                
                td.pixelFormat = fmt;
                td.width = _tcp.width;
                td.height = _tcp.height;
                td.depth = _tcp.num_arrays;
                td.textureType = MTLTextureType3D;
                td.mipmapLevelCount = _tcp.num_mips;
                td.arrayLength = 1;
                td.sampleCount = _tcp.sample_count;
                
                // arrays become slices
                num_slices = td.depth;
            }
            
            td.usage = to_metal_texture_usage(_tcp.bind_flags);
            td.storageMode = to_metal_storage_mode(_tcp);
            
            texture = [_metal_device newTextureWithDescriptor:td];
            
            if (tcp.data)
            {
                u8* mip_data = (u8*)tcp.data;
                
                for(u32 a = 0; a <  num_arrays; ++a)
                {
                    u32 mip_w = tcp.width;
                    u32 mip_h = tcp.height;
                    u32 mip_d = num_slices;
                    
                    for (u32 i = 0; i < tcp.num_mips; ++i)
                    {
                        u32       pitch = _tcp.block_size * _tcp.pixels_per_block * mip_w;
                        u32       depth_pitch = pitch * mip_h * mip_d;
                        
                        MTLRegion region = MTLRegionMake2D(0, 0, mip_w, mip_h);
                        
                        [texture replaceRegion:region
                                   mipmapLevel:i
                                         slice:a
                                     withBytes:mip_data
                                   bytesPerRow:pitch
                                 bytesPerImage:depth_pitch];
                        
                        mip_data += depth_pitch;
                        
                        // images may be non-pot
                        mip_w /= 2;
                        mip_h /= 2;
                        
                        mip_w = max<u32>(1, mip_w);
                        mip_h = max<u32>(1, mip_h);
                    }
                    
                    mip_d /= 2;
                    mip_d = max<u32>(1, mip_d);
                }
            }

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).texture = {texture, fmt};
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

            if (texture_index == 0)
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
            // todo...
        }

        void renderer_set_rasterizer_state(u32 rasterizer_state_index)
        {
            // todo...
        }

        void renderer_set_viewport(const viewport& vp)
        {
            _state.viewport = (MTLViewport){vp.x, vp.y, vp.width, vp.height, vp.min_depth, vp.max_depth};
        }

        void renderer_set_scissor_rect(const rect& r)
        {
            _state.scissor =
                (MTLScissorRect){(u32)r.left, (u32)r.top, (u32)r.right - (u32)r.left, (u32)r.bottom - (u32)r.top};
        }

        void renderer_create_blend_state(const blend_creation_params& bcp, u32 resource_slot)
        {
            _res_pool.insert(resource(), resource_slot);
            metal_blend_state& blend = _res_pool.get(resource_slot).blend;

            // todo is this the right place?
            blend.alpha_to_coverage_enable = bcp.alpha_to_coverage_enable;

            blend.num_render_targets = bcp.num_render_targets;

            for (u32 i = 0; i < blend.num_render_targets; ++i)
            {
                blend.attachment[i].enabled = bcp.render_targets[i].blend_enable;
                blend.attachment[i].write_mask = bcp.render_targets[i].render_target_write_mask;

                blend.attachment[i].rgb_op = to_metal_blend_op(bcp.render_targets[i].blend_op);
                blend.attachment[i].src_rgb_factor = to_metal_blend_factor(bcp.render_targets[i].src_blend);
                blend.attachment[i].dst_rgb_factor = to_metal_blend_factor(bcp.render_targets[i].dest_blend);

                if (bcp.independent_blend_enable)
                {
                    blend.attachment[i].alpha_op = to_metal_blend_op(bcp.render_targets[i].blend_op_alpha);
                    blend.attachment[i].src_alpha_factor = to_metal_blend_factor(bcp.render_targets[i].src_blend_alpha);
                    blend.attachment[i].dst_alpha_factor = to_metal_blend_factor(bcp.render_targets[i].dest_blend_alpha);
                }
                else
                {
                    blend.attachment[i].alpha_op = blend.attachment[i].rgb_op;
                    blend.attachment[i].src_alpha_factor = blend.attachment[i].src_rgb_factor;
                    blend.attachment[i].dst_alpha_factor = blend.attachment[i].dst_rgb_factor;
                }
            }
        }

        void renderer_set_blend_state(u32 blend_state_index)
        {
            _state.blend_state = blend_state_index;
        }

        void renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot)
        {
            _res_pool.insert(resource(), resource_slot);
            
            MTLDepthStencilDescriptor* dsd = [MTLDepthStencilDescriptor new];
            dsd.depthCompareFunction = to_metal_compare_function(dscp.depth_func);
            dsd.depthWriteEnabled = dscp.depth_write_mask > 0 ? YES : NO;
            
            _res_pool.get(resource_slot).depth_stencil = [_metal_device newDepthStencilStateWithDescriptor:dsd];
        }

        void renderer_set_depth_stencil_state(u32 depth_stencil_state)
        {
            _state.depth_stencil = _res_pool.get(depth_stencil_state).depth_stencil;
        }

        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
        {
            validate_encoder();
            bind_pipeline();

            // draw calls
            [_state.render_encoder drawPrimitives:to_metal_primitive_type(primitive_topology)
                                      vertexStart:start_vertex
                                      vertexCount:vertex_count];
        }

        static pen_inline void _indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                                  u32 base_vertex, u32 primitive_topology)
        {
            validate_encoder();
            bind_pipeline();

            u32 offset = (start_index * _state.index_buffer.size_bytes);
            
            // draw calls
            [_state.render_encoder drawIndexedPrimitives:to_metal_primitive_type(primitive_topology)
                                              indexCount:index_count
                                               indexType:_state.index_buffer.type
                                             indexBuffer:_state.index_buffer.buffer
                                       indexBufferOffset:_state.index_buffer.offset + offset
                                           instanceCount:instance_count
                                              baseVertex:base_vertex
                                            baseInstance:start_instance];
        }

        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
        {
            _indexed_instanced(1, 0, index_count, start_index, base_vertex, primitive_topology);
        }

        void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                             u32 base_vertex, u32 primitive_topology)
        {
            _indexed_instanced(instance_count, start_instance, index_count, start_index, base_vertex, primitive_topology);
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
            if (_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
            }

            _state.pass = [MTLRenderPassDescriptor renderPassDescriptor];
            
            _state.formats.colour_attachments[0] = MTLPixelFormatInvalid;
            _state.formats.depth_attachment = MTLPixelFormatInvalid;
            _state.formats.sample_count = 1;
            
            if (num_colour_targets == 1 && colour_targets[0] == PEN_BACK_BUFFER_COLOUR)
            {
                // backbuffer colour target
                _state.drawable = _metal_view.currentDrawable;
                _state.formats.sample_count = _metal_view.sampleCount;

                if(_state.formats.sample_count > 1)
                {
                    // msaa
                    _state.pass.colorAttachments[0].texture = _metal_view.multisampleColorTexture;
                    _state.pass.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
                    _state.pass.colorAttachments[0].resolveTexture = _state.drawable.texture;
                }
                else
                {
                    // non msaa
                    _state.pass.colorAttachments[0].texture = _state.drawable.texture;
                    _state.pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                }
                
                _state.formats.colour_attachments[0] = _metal_view.colorPixelFormat;
            }
            else
            {
                // multiple render targets
                for (u32 i = 0; i < num_colour_targets; ++i)
                {
                    id<MTLTexture> texture = _res_pool.get(colour_targets[i]).texture.tex;

                    _state.pass.colorAttachments[i].texture = texture;
                    _state.pass.colorAttachments[i].loadAction = MTLLoadActionDontCare;
                    _state.pass.colorAttachments[i].storeAction = MTLStoreActionStore;

                    _state.formats.colour_attachments[i] = _res_pool.get(colour_targets[i]).texture.fmt;
                }
                
                // todo msaa rt
            }
            
            if(depth_target == PEN_BACK_BUFFER_DEPTH)
            {
                _state.pass.depthAttachment.texture = _metal_view.depthStencilTexture;
                _state.formats.depth_attachment = _metal_view.depthStencilPixelFormat;
                
                _state.pass.depthAttachment.loadAction = MTLLoadActionDontCare;
                _state.pass.depthAttachment.storeAction = MTLStoreActionStore;
            }
            else if(is_valid(depth_target))
            {
                _state.pass.depthAttachment.texture = _res_pool.get(depth_target).texture.tex;

                _state.pass.depthAttachment.loadAction = MTLLoadActionDontCare;
                _state.pass.depthAttachment.storeAction = MTLStoreActionStore;
                
                _state.formats.depth_attachment = _res_pool.get(depth_target).texture.fmt;
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
            if (rrbp.resource_index == 0)
            {
            }
        }

        void renderer_present()
        {
            if (_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
            }

            // flush cmd buf and present
            [_state.cmd_buffer presentDrawable:_state.drawable];
            [_state.cmd_buffer commit];

            // null state for next frame
            _state.cmd_buffer = nil;
            _state.pipeline_hash = 0;
        }

        void renderer_push_perf_marker(const c8* name)
        {
        }

        void renderer_pop_perf_marker()
        {
        }

        void renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
        {
            switch (type)
            {
                case RESOURCE_TEXTURE:
                    direct::renderer_release_texture(dest);
                    break;
                case RESOURCE_BUFFER:
                    direct::renderer_release_buffer(dest);
                    break;
                case RESOURCE_VERTEX_SHADER:
                    direct::renderer_release_shader(dest, PEN_SHADER_TYPE_VS);
                    break;
                case RESOURCE_PIXEL_SHADER:
                    direct::renderer_release_shader(dest, PEN_SHADER_TYPE_PS);
                    break;
                case RESOURCE_RENDER_TARGET:
                    direct::renderer_release_render_target(dest);
                    break;
                default:
                    break;
            }
            
            memcpy(&_res_pool[dest], &_res_pool[src], sizeof(resource));
        }

        void renderer_release_shader(u32 shader_index, u32 shader_type)
        {
            _res_pool.get(shader_index).shader.lib = nil;
        }

        void renderer_release_clear_state(u32 clear_state)
        {
            // no allocs
        }

        void renderer_release_buffer(u32 buffer_index)
        {
            _res_pool.get(buffer_index).buffer._data[0] = nil;
            _res_pool.get(buffer_index).buffer._data[1] = nil;
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
            _res_pool.get(render_target).texture.tex = nil;
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

// renderer_metal.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "renderer.h"
#include "renderer_shared.h"
#include "console.h"
#include "data_struct.h"
#include "hash.h"
#include "str/Str.h"
#include "threads.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Availability.h>

using namespace pen;

extern window_creation_params pen_window;

#define NBB 3                 // buffers to prevent locking gpu / cpu on dynamic buffers.
#define CBUF_OFFSET 4         // from pmfx.. offset of cbuffers to prevent collisions with vertex buffers

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
        u32            num_targets;
        MTLPixelFormat colour_attachments[pen::MAX_MRT];
        MTLPixelFormat depth_attachment;
        u32            sample_count;
    };
    
    struct cached_pipeline
    {
        hash_id hash;
        id<MTLRenderPipelineState> pipeline;
    };

    struct current_state
    {
        // metal
        id<MTLRenderPipelineState>   pipeline;
        id<MTLCommandQueue>          command_queue;
        id<MTLRenderCommandEncoder>  render_encoder;
        id<MTLComputeCommandEncoder> compute_encoder;
        id<MTLBlitCommandEncoder>    blit_encoder;
        id<MTLCommandBuffer>         cmd_buffer;
        id<CAMetalDrawable>          drawable;
        MTLRenderPassDescriptor*     pass = nil;
        dispatch_semaphore_t         completion;
        index_buffer_cmd             index_buffer;
        u32                          input_layout;
        
        // hashable for pass
        u32*                         colour_targets = nullptr;
        u32                          depth_target;
        u32                          colour_slice;
        u32                          depth_slice;
        u32                          clear_state;

        // hashable to set on command encoder
        MTLViewport                 viewport;
        MTLScissorRect              scissor;
        id<MTLDepthStencilState>    depth_stencil;
        u32                         raster_state;
        u8                          stencil_ref;

        // hashable to rebuild pipe
        pixel_formats               formats;
        MTLVertexDescriptor*        vertex_descriptor;
        id<MTLFunction>             vertex_shader;
        id<MTLFunction>             fragment_shader;
        id<MTLFunction>             compute_shader;
        u32                         blend_state;
        bool                        stream_out;

        // cache
        hash_id pipeline_hash = 0;
        hash_id encoder_hash = 0;
        hash_id target_hash = 0;
        cached_pipeline* cached_pipelines = nullptr;
    };

    struct shader_resource
    {
        id<MTLLibrary>  lib;
        u32             type;
        id<MTLFunction> func;
    };

    struct texture_resource
    {
        id<MTLTexture>          tex;
        id<MTLTexture>          tex_msaa;
        MTLPixelFormat          fmt;
        MTLPixelFormat          resolve_fmt;
        u32                     samples;
        texture_creation_params tcp;
        u32                     num_mips;
        u32                     invalidate;
    };

    struct metal_clear_state
    {
        u32           num_colour_targets;
        MTLClearColor colour[pen::MAX_MRT];
        MTLLoadAction colour_load_action[pen::MAX_MRT];
        f32           depth_clear;
        u32           stencil_clear;
        MTLLoadAction depth_load_action;
        MTLLoadAction stencil_load_action;
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

    struct metal_raster_state
    {
        MTLCullMode         cull_mode;
        MTLTriangleFillMode fill_mode;
        MTLWinding          winding;
        bool                scissor_enabled;
    };

    struct dynamic_buffer
    {
        id<MTLBuffer>                         static_buffer;    // static data never updated
        pen::multi_buffer<id<MTLBuffer>, NBB> dynamic_buffers;  // data updated once per frame
        stretchy_dynamic_buffer*              stretchy_buffer;  // data updated multiple times per frame
        size_t                                _dynamic_read_offset = 0;
        u32                                   _frame_writes;
        u32                                   _buffer_size;
        u32                                   _options;
        u32                                   _static;
        u32                                   frame;

        id<MTLBuffer>   read(size_t& offset);
        id<MTLBuffer>   read();
        void            init(id<MTLBuffer>* bufs, u32 num_buffers, u32 buffer_size, u32 options, u32 bind_flags);
        void            release();
        void            update(const void* data, u32 data_size, u32 offset);
    };

    struct resource
    {
        u32 type;

        union {
            dynamic_buffer           buffer;
            texture_resource         texture;
            id<MTLSamplerState>      sampler;
            shader_resource          shader;
            metal_clear_state        clear;
            MTLVertexDescriptor*     vertex_descriptor;
            metal_blend_state        blend;
            id<MTLDepthStencilState> depth_stencil;
            metal_raster_state       raster_state;
        };

        resource(){};
    };

    // would like to make these into a ctx_ struct
    id<MTLDevice>      _metal_device;
    res_pool<resource> _res_pool;
    MTKView*           _metal_view;
    current_state      _state;
    a_u64              _frame_sync;
    a_u64              _resize_sync;
    
    // dynamic buffer impl
    id<MTLBuffer> dynamic_buffer::read()
    {
        if (_static)
            return static_buffer;
            
        if(_frame_writes > 1)
        {
            _res_pool.get(stretchy_buffer->_gpu_buffer).buffer.dynamic_buffers.backbuffer();
        }
            
        return dynamic_buffers.backbuffer();
    }
    
    id<MTLBuffer> dynamic_buffer::read(size_t& offset)
    {
        offset = 0;
        
        if (_static)
            return static_buffer;
            
        if(_frame_writes > 1)
        {
            offset = _dynamic_read_offset;
            return _res_pool.get(stretchy_buffer->_gpu_buffer).buffer.dynamic_buffers.backbuffer();
        }
            
        return dynamic_buffers.backbuffer();
    }

    void dynamic_buffer::init(id<MTLBuffer>* bufs, u32 num_buffers, u32 buffer_size, u32 options, u32 bind_flags)
    {
        _buffer_size = buffer_size;
        _options = options;
        _static = 0;
        _frame_writes = 0;
        
        stretchy_buffer = _renderer_get_stretchy_dynamic_buffer(bind_flags);

        if (num_buffers == 1)
        {
            _static = 1;
            static_buffer = bufs[0];
        }
        else
        {
            pen::multi_buffer<id<MTLBuffer>, NBB>& db = dynamic_buffers;
            db._fb = 0;
            db._bb = num_buffers - 1;
            db._swaps = 0;
            db._frame = 0;

            for (u32 i = 0; i < num_buffers; ++i)
                db._data[i] = {bufs[i]};
        }
    }

    void dynamic_buffer::release()
    {
        static_buffer = nil;
        for (u32 i = 0; i < 10; ++i)
            for (u32 j = 0; j < NBB; ++j)
                dynamic_buffers._data[j] = nil;
    }

    void dynamic_buffer::update(const void* data, u32 data_size, u32 offset)
    {
        u32 cur_frame = _renderer_frame_index();
        
        if (frame != cur_frame)
        {
            frame = cur_frame;
            _frame_writes = 0;
        }
        
        if(_frame_writes > 0)
        {
            _dynamic_read_offset = _renderer_buffer_multi_update(stretchy_buffer, data, (size_t)data_size);
        }
        else
        {
            auto& db = dynamic_buffers;
            u32 c = db._swaps == 0 ? NBB : 1;

            // swap once a frame
            if (cur_frame != db._frame)
            {
                db._frame = cur_frame;
                db.swap_buffers();
            }

            for (u32 i = 0; i < c; ++i)
            {
                id<MTLBuffer>& bb = db.backbuffer();

                u8* pdata = (u8*)[bb contents];
                pdata = pdata + offset;

                memcpy(pdata, data, data_size);

                // first update
                if (c == NBB)
                    db.swap_buffers();
            }
        }
        
        _frame_writes++;
    }
}

namespace // pen consts -> metal consts
{
    MTLVertexFormat to_metal_vertex_format(u32 pen_vertex_format)
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
                return MTLVertexFormatUChar4;
            case PEN_VERTEX_FORMAT_UNORM2:
                return MTLVertexFormatUChar2;
            case PEN_VERTEX_FORMAT_UNORM1:
                return MTLVertexFormatUChar;
        }

        // unhandled
        PEN_ASSERT(0);
        return MTLVertexFormatInvalid;
    }
    
    pen_inline MTLIndexType to_metal_index_format(u32 pen_index_format)
    {
        return pen_index_format == PEN_FORMAT_R16_UINT ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
    }

    pen_inline u32 index_size_bytes(u32 pen_index_type)
    {
        if (pen_index_type == PEN_FORMAT_R16_UINT)
            return 2;

        return 4;
    }

    MTLPixelFormat to_metal_pixel_format(u32 pen_pixel_format)
    {
        switch (pen_pixel_format)
        {
            case PEN_TEX_FORMAT_RGBA8_UNORM:
                return MTLPixelFormatRGBA8Unorm;
            case PEN_TEX_FORMAT_BGRA8_UNORM:
                return MTLPixelFormatBGRA8Unorm;
            case PEN_TEX_FORMAT_R32G32B32A32_FLOAT:
                return MTLPixelFormatRGBA32Float;
            case PEN_TEX_FORMAT_R32G32_FLOAT:
                return MTLPixelFormatRG32Float;
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
#ifndef PEN_PLATFORM_IOS
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
#else
            case PEN_TEX_FORMAT_D24_UNORM_S8_UINT:
                return MTLPixelFormatDepth32Float_Stencil8;
                break;
#endif
        }

        // unhandled
        PEN_ASSERT(0);
        return MTLPixelFormatInvalid;
    }

    MTLTextureUsage to_metal_texture_usage(u32 bind_flags)
    {
        MTLTextureUsage usage = 0;
        if (bind_flags & PEN_BIND_SHADER_RESOURCE)
            usage |= MTLTextureUsageShaderRead;

        if (bind_flags & PEN_BIND_RENDER_TARGET)
            usage |= MTLTextureUsageRenderTarget;

        if (bind_flags & PEN_BIND_DEPTH_STENCIL)
            usage |= MTLTextureUsageRenderTarget;

        if (bind_flags & PEN_BIND_SHADER_WRITE)
            usage |= MTLTextureUsageShaderWrite;

        return usage;
    }

    MTLStorageMode to_metal_storage_mode(const texture_creation_params& tcp)
    {
        if (tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT || tcp.sample_count > 1)
            return MTLStorageModePrivate;
#ifndef PEN_PLATFORM_IOS
        return MTLStorageModeManaged;
#else
        return MTLStorageModeShared;
#endif
    }

    MTLSamplerAddressMode to_metal_sampler_address_mode(u32 address_mode)
    {
        switch (address_mode)
        {
            case PEN_TEXTURE_ADDRESS_WRAP:
                return MTLSamplerAddressModeRepeat;
            case PEN_TEXTURE_ADDRESS_MIRROR:
                return MTLSamplerAddressModeMirrorRepeat;
            case PEN_TEXTURE_ADDRESS_CLAMP:
                return MTLSamplerAddressModeClampToEdge;
#ifndef PEN_PLATFORM_IOS
            case PEN_TEXTURE_ADDRESS_MIRROR_ONCE:
                return MTLSamplerAddressModeMirrorClampToEdge;
#endif
        }

        // unhandled
        PEN_ASSERT(0);
        return MTLSamplerAddressModeRepeat;
    }

    MTLSamplerMinMagFilter to_metal_min_mag_filter(u32 filter)
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

    MTLSamplerMipFilter to_metal_mip_filter(u32 filter)
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

    const char* get_metal_version_string()
    {
#ifndef PEN_PLATFORM_IOS
        if ([_metal_device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v2])
        {
            return "Metal MacOS 2.0";
        }
        else if ([_metal_device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v1])
        {
            return "Metal MacOS 1.0";
        }
#else

#endif
        return "";
    }

    MTLPrimitiveType to_metal_primitive_type(u32 pt)
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

    MTLBlendOperation to_metal_blend_op(u32 bo)
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

    MTLBlendFactor to_metal_blend_factor(u32 bf)
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

    MTLCompareFunction to_metal_compare_function(u32 cf)
    {
        switch (cf)
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

    MTLCullMode to_metal_cull_mode(u32 cull_mode)
    {
        switch (cull_mode)
        {
            case PEN_CULL_FRONT:
                return MTLCullModeFront;
            case PEN_CULL_BACK:
                return MTLCullModeBack;
            case PEN_CULL_NONE:
                return MTLCullModeNone;
        }

        PEN_ASSERT(0);
        return MTLCullModeNone;
    }

    MTLTriangleFillMode to_metal_fill_mode(u32 fill_mode)
    {
        switch (fill_mode)
        {
            case PEN_FILL_SOLID:
                return MTLTriangleFillModeFill;
            case PEN_FILL_WIREFRAME:
                return MTLTriangleFillModeLines;
        }

        PEN_ASSERT(0);
        return MTLTriangleFillModeFill;
    }

    pen_inline MTLWinding to_metal_winding(u32 front_ccw)
    {
        if (front_ccw)
            return MTLWindingCounterClockwise;

        return MTLWindingClockwise;
    }

    MTLStencilOperation to_metal_stencil_op(u32 stencil_op)
    {
        switch (stencil_op)
        {
            case PEN_STENCIL_OP_DECR:
                return MTLStencilOperationDecrementWrap;
            case PEN_STENCIL_OP_INCR:
                return MTLStencilOperationIncrementWrap;
            case PEN_STENCIL_OP_DECR_SAT:
                return MTLStencilOperationDecrementClamp;
            case PEN_STENCIL_OP_INCR_SAT:
                return MTLStencilOperationIncrementClamp;
            case PEN_STENCIL_OP_INVERT:
                return MTLStencilOperationInvert;
            case PEN_STENCIL_OP_KEEP:
                return MTLStencilOperationKeep;
            case PEN_STENCIL_OP_REPLACE:
                return MTLStencilOperationReplace;
            case PEN_STENCIL_OP_ZERO:
                return MTLStencilOperationZero;
        }

        PEN_ASSERT(0);
        return MTLStencilOperationKeep;
    }
    
    void bind_render_pass()
    {
        u32 num_colour_targets = sb_count(_state.colour_targets);
        if(num_colour_targets == 0 && _state.depth_target == PEN_INVALID_HANDLE)
            return;
        
        HashMurmur2A hh;
        hh.begin();
        hh.add(num_colour_targets);
        hh.add(_state.depth_target);
        hh.add(_state.colour_slice);
        hh.add(_state.depth_slice);
        hh.add(&_state.colour_targets[0], sizeof(u32)*num_colour_targets);
        hh.add(_state.clear_state);
        hash_id cur = hh.end();
        if (cur == _state.target_hash)
            return;
        _state.target_hash = cur;
        
        // create new cmd buffer
        if (_state.cmd_buffer == nil)
            _state.cmd_buffer = [_state.command_queue commandBuffer];
                            
        // finish render encoding
        if (_state.render_encoder)
        {
            [_state.render_encoder endEncoding];
            _state.render_encoder = nil;
            _state.pipeline_hash = 0;
        }

        // create new cmd buffer
        if (_state.cmd_buffer == nil)
            _state.cmd_buffer = [_state.command_queue commandBuffer];
                            
        // finish render encoding
        if (_state.render_encoder)
        {
            [_state.render_encoder endEncoding];
            _state.render_encoder = nil;
            _state.pipeline_hash = 0;
        }
        
        u32* colour_targets = _state.colour_targets;
        u32 colour_slice = _state.colour_slice;
        u32 depth_target = _state.depth_target;
        u32 depth_slice = _state.depth_slice;

        _state.pass = [MTLRenderPassDescriptor renderPassDescriptor];

        _state.formats.colour_attachments[0] = MTLPixelFormatInvalid;
        _state.formats.depth_attachment = MTLPixelFormatInvalid;
        _state.formats.sample_count = 1;
        _state.formats.num_targets = num_colour_targets;
        
        bool backbuffer = ((num_colour_targets == 1 && colour_targets[0] == PEN_BACK_BUFFER_COLOUR)
                           || depth_target == PEN_BACK_BUFFER_DEPTH);

        if (backbuffer)
        {
            _state.drawable = _metal_view.currentDrawable;
            _state.formats.sample_count = (u32)_metal_view.sampleCount;
            
            if((num_colour_targets == 1 && colour_targets[0] == PEN_BACK_BUFFER_COLOUR))
            {
                if (_state.formats.sample_count > 1)
                {
                    // msaa
                    _state.pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
                    _state.pass.colorAttachments[0].texture = _metal_view.multisampleColorTexture;
                    _state.pass.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
                    _state.pass.colorAttachments[0].resolveTexture = _state.drawable.texture;
                }
                else
                {
                    // non msaa
                    _state.pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
                    _state.pass.colorAttachments[0].texture = _state.drawable.texture;
                    _state.pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                }
                
                _state.formats.colour_attachments[0] = _metal_view.colorPixelFormat;
            }

            if(depth_target == PEN_BACK_BUFFER_DEPTH)
            {
                _state.formats.depth_attachment = _metal_view.depthStencilPixelFormat;
                
                _state.pass.depthAttachment.texture = _metal_view.depthStencilTexture;
                _state.pass.depthAttachment.loadAction = MTLLoadActionLoad;
                _state.pass.depthAttachment.storeAction = MTLStoreActionStore;
                
                _state.pass.stencilAttachment.texture = _metal_view.depthStencilTexture;
                _state.pass.stencilAttachment.loadAction = MTLLoadActionLoad;
                _state.pass.stencilAttachment.storeAction = MTLStoreActionStore;
            }
        }
        else
        {
            // multiple render targets
            for (u32 i = 0; i < num_colour_targets; ++i)
            {
                resource& r = _res_pool.get(colour_targets[i]);
                if (r.texture.num_mips > 1)
                    r.texture.invalidate = 1;
                
                id<MTLTexture> texture = r.texture.tex;
                if (r.texture.samples > 1)
                    texture = r.texture.tex_msaa;
                
                _state.formats.sample_count = r.texture.samples;
                
                _state.pass.colorAttachments[i].slice = colour_slice;
                _state.pass.colorAttachments[i].texture = texture;
                _state.pass.colorAttachments[i].loadAction = MTLLoadActionLoad;
                _state.pass.colorAttachments[i].storeAction = MTLStoreActionStore;
                
                _state.formats.colour_attachments[i] = r.texture.fmt;
            }
            _state.formats.num_targets = num_colour_targets;
            
            if (is_valid(depth_target))
            {
                resource&      r = _res_pool.get(depth_target);
                id<MTLTexture> texture = r.texture.tex;
                if (r.texture.samples > 1)
                    texture = r.texture.tex_msaa;

                if (r.texture.num_mips > 1)
                    r.texture.invalidate = 1;

                _state.formats.sample_count = r.texture.samples;

                _state.pass.depthAttachment.slice = depth_slice;
                _state.pass.depthAttachment.texture = texture;
                _state.pass.depthAttachment.loadAction = MTLLoadActionLoad;
                _state.pass.depthAttachment.storeAction = MTLStoreActionStore;

                _state.pass.stencilAttachment.slice = depth_slice;
                _state.pass.stencilAttachment.texture = texture;
                _state.pass.stencilAttachment.loadAction = MTLLoadActionLoad;
                _state.pass.stencilAttachment.storeAction = MTLStoreActionStore;

                _state.formats.depth_attachment = r.texture.fmt;
            }
        }
        
        // clear
        metal_clear_state& clear = _res_pool.get(_state.clear_state).clear;
        
        for (u32 c = 0; c < clear.num_colour_targets; ++c)
        {
            _state.pass.colorAttachments[c].loadAction = clear.colour_load_action[c];
            _state.pass.colorAttachments[c].clearColor = clear.colour[c];
        }

        if (_state.pass.depthAttachment)
        {
            _state.pass.depthAttachment.loadAction = clear.depth_load_action;
            _state.pass.depthAttachment.clearDepth = clear.depth_clear;
            _state.pass.stencilAttachment.loadAction = clear.stencil_load_action;
            _state.pass.stencilAttachment.clearStencil = clear.stencil_clear;
        }
    }
    
    void validate_blit_encoder()
    {
        if (_state.cmd_buffer == nil)
            _state.cmd_buffer = [_state.command_queue commandBuffer];

        if (!_state.blit_encoder)
        {
            _state.blit_encoder = [_state.cmd_buffer blitCommandEncoder];
        }
    }

    void validate_render_encoder()
    {
        bind_render_pass();
        
        if (!_state.render_encoder)
        {
            _state.render_encoder = [_state.cmd_buffer renderCommandEncoderWithDescriptor:_state.pass];
            _state.encoder_hash = 0;
        }

        // only set if we need to: dss, vp, raster and scissor..
        HashMurmur2A hh;
        hh.begin();
        static const size_t off = (u8*)&_state.formats - (u8*)&_state.viewport;
        hh.add(&_state.viewport, off);

        hash_id cur = hh.end();
        if (cur == _state.encoder_hash)
            return;

        _state.encoder_hash = cur;

        // vp
        [_state.render_encoder setViewport:_state.viewport];
        
        // dss
        if (_state.depth_stencil && _state.formats.depth_attachment != MTLPixelFormatInvalid)
        {
            [_state.render_encoder setDepthStencilState:_state.depth_stencil];
            [_state.render_encoder setStencilReferenceValue:_state.stencil_ref];
            [_state.render_encoder setStencilFrontReferenceValue:_state.stencil_ref
                                              backReferenceValue:_state.stencil_ref];
        }
        
        // raster
        metal_raster_state& rs = _res_pool.get(_state.raster_state).raster_state;
        [_state.render_encoder setCullMode:rs.cull_mode];
        [_state.render_encoder setTriangleFillMode:rs.fill_mode];
        [_state.render_encoder setFrontFacingWinding:rs.winding];
        
        // scissor
        if (rs.scissor_enabled)
            [_state.render_encoder setScissorRect:_state.scissor];
    }

    void validate_compute_encoder()
    {
        if (_state.cmd_buffer == nil)
            _state.cmd_buffer = [_state.command_queue commandBuffer];

        if (!_state.compute_encoder)
            _state.compute_encoder = [_state.cmd_buffer computeCommandEncoder];
    }

    void bind_render_pipeline()
    {
        // pipeline hash
        HashMurmur2A hh;
        hh.begin();
        static const size_t off = (u8*)&_state.pipeline_hash - (u8*)&_state.formats;
        hh.add(&_state.formats, off);

        hash_id cur = hh.end();
        if (cur == _state.pipeline_hash)
            return;
        
        // set current hash
        _state.pipeline_hash = cur;
        
        // look for exisiting
        u32 num_chached = sb_count(_state.cached_pipelines);
        for(u32 i = 0; i < num_chached; ++i)
        {
            if(_state.cached_pipelines[i].hash == cur)
            {
                [_state.render_encoder setRenderPipelineState:_state.cached_pipelines[i].pipeline];
                return;
            }
        }

        // create a new pipeline
        MTLRenderPipelineDescriptor* pipeline_desc = [MTLRenderPipelineDescriptor new];

        pipeline_desc.vertexFunction = _state.vertex_shader;
        pipeline_desc.fragmentFunction = _state.fragment_shader;
        pipeline_desc.sampleCount = _state.formats.sample_count;

        for (u32 i = 0; i < _state.formats.num_targets; ++i)
            pipeline_desc.colorAttachments[i].pixelFormat = _state.formats.colour_attachments[i];

        pipeline_desc.depthAttachmentPixelFormat = _state.formats.depth_attachment;
        pipeline_desc.stencilAttachmentPixelFormat = _state.formats.depth_attachment;

        // apply blend state
        metal_blend_state& blend = _res_pool.get(_state.blend_state).blend;
        for (u32 i = 0; i < blend.num_render_targets; ++i)
        {
            if (i >= _state.formats.num_targets)
                continue;

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

        if (_state.stream_out)
        {
            pipeline_desc.rasterizationEnabled = NO;
        }

        pipeline_desc.vertexDescriptor = _state.vertex_descriptor;

        NSError*                   error = nil;
        id<MTLRenderPipelineState> pipeline =
            [_metal_device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
        
        //add to cache
        cached_pipeline cp;
        cp.hash = cur;
        cp.pipeline = pipeline;
        sb_push(_state.cached_pipelines, cp);

        [_state.render_encoder setRenderPipelineState:pipeline];
    }

    void bind_compute_pipeline()
    {
        NSError*                    error = nil;
        id<MTLComputePipelineState> pipeline =
            [_metal_device newComputePipelineStateWithFunction:_state.compute_shader error:&error];

        [_state.compute_encoder setComputePipelineState:pipeline];
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
        info.renderer_cmd = "-renderer metal";
        info.renderer = device_name.c_str();
        info.vendor = device_name.c_str();

        // macos caps.. todo ios
        info.caps |= PEN_CAPS_TEX_FORMAT_BC1;
        info.caps |= PEN_CAPS_TEX_FORMAT_BC2;
        info.caps |= PEN_CAPS_TEX_FORMAT_BC3;
        info.caps |= PEN_CAPS_TEX_FORMAT_BC4;
        info.caps |= PEN_CAPS_TEX_FORMAT_BC5;
        info.caps |= PEN_CAPS_COMPUTE;

        return info;
    }

    namespace direct
    {
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res)
        {
            PEN_ASSERT(params); // params must be a pointer to MTKView
            _metal_view = (MTKView*)params;
            _metal_device = _metal_view.device;

            _state.command_queue = [_metal_device newCommandQueue];
            _state.render_encoder = 0;

            // reserve space for some resources
            _res_pool.init(128);
            _frame_sync = 0;
            _resize_sync = 1;
            
            // create a backbuffer / swap chain, this will be blittled to the drawable on present
            pen::texture_creation_params tcp = {0};
            tcp.width = (u32)pen_window.width;
            tcp.height = (u32)pen_window.height;
            tcp.sample_count = 1;
            tcp.cpu_access_flags = 0;
            tcp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
            tcp.num_arrays = 1;
            tcp.num_mips = 1;
            tcp.bind_flags = PEN_BIND_RENDER_TARGET | PEN_BIND_SHADER_RESOURCE;
            tcp.pixels_per_block = 1;
            tcp.sample_quality = 0;
            tcp.block_size = 32;
            tcp.usage = PEN_USAGE_DEFAULT;
            tcp.flags = 0;
            
            // colour buffer
            renderer_create_render_target(tcp, bb_res);
                        
            // depth buffer
            tcp.format = PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
            renderer_create_render_target(tcp, bb_depth_res);

            // frame completion sem
            _state.completion = dispatch_semaphore_create(NBB);
            dispatch_semaphore_signal(_state.completion);
            
            // shared init (stretchy buffer, resolve resources etc)
            _renderer_shared_init();

            return 1;
        }

        void renderer_shutdown()
        {
            // nothing to do yet
        }

        void renderer_make_context_current()
        {
            // to remove
        }
        
        void renderer_new_frame()
        {
            _frame_sync = _renderer_frame_index();
            _renderer_new_frame();
        }
        
        void renderer_end_frame()
        {
            _renderer_end_frame();
        }

        void renderer_sync()
        {
            while (_frame_sync == _renderer_frame_index())
                thread_sleep_us(100);
                
            _resize_sync = _renderer_resize_index();
        }
        
        bool renderer_frame_valid()
        {
            return _resize_sync.load() == _renderer_resize_index();
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
                if (mc.num_colour_targets == 0)
                    mc.num_colour_targets = 1;
            }

            // colour
            mc.colour[0] = MTLClearColorMake(cs.r, cs.g, cs.b, cs.a);

            // mrt
            for (u32 i = 0; i < cs.num_colour_targets; ++i)
            {
                mc.colour_load_action[i] = mc.colour_load_action[0];

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

            mc.stencil_load_action = MTLLoadActionLoad;
            if (cs.flags & PEN_CLEAR_STENCIL_BUFFER)
                mc.stencil_load_action = MTLLoadActionClear;

            mc.depth_clear = cs.depth;
            mc.stencil_clear = (u32)cs.stencil;
        }

        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {
            _state.clear_state = clear_state_index;
            validate_render_encoder();
        }

        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot)
        {
            id<MTLLibrary> lib;
            
            static const c8* c = "MTLB";
            bool bin = memcmp(params.byte_code, &c[0], 4) == 0;
            if(!bin) // compile source
            {
                const c8* csrc = (const c8*)params.byte_code;
                NSString* str = [[NSString alloc] initWithBytes:csrc length:params.byte_code_size encoding:NSASCIIStringEncoding];
                
                NSError*           err = nil;
                MTLCompileOptions* opts = [MTLCompileOptions alloc];
                opts.fastMathEnabled = YES;
                
                lib = [_metal_device newLibraryWithSource:str options:opts error:&err];
                
                if (err)
                {
                    if (err.code == 3)
                    {
                        NSLog(@" error => %@ ", err);
                    }
                }
            }
            else
            {
                u8* new_data = new u8[params.byte_code_size+1];
                memcpy(new_data, params.byte_code, params.byte_code_size);
                new_data[params.byte_code_size] = '\0';
                
                
                NSError* err = nil;
                dispatch_data_t dd = dispatch_data_create(new_data, params.byte_code_size, dispatch_get_main_queue(), ^{});
                lib = [_metal_device newLibraryWithData:dd error:&err];
                
                if (err)
                {
                    NSLog(@" error => %@ ", err);
                }
            }

            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).shader = {lib, params.type, nil};
        }

        void renderer_set_shader(u32 shader_index, u32 shader_type)
        {
            shader_resource& res = _res_pool.get(shader_index).shader;

            _state.stream_out = false;

            switch (shader_type)
            {
                case PEN_SHADER_TYPE_VS:
                    if (!res.func)
                        res.func = [res.lib newFunctionWithName:@"vs_main"];
                    _state.vertex_shader = res.func;
                    PEN_ASSERT(_state.vertex_shader);
                    break;
                case PEN_SHADER_TYPE_PS:
                    if (!res.func)
                        res.func = [res.lib newFunctionWithName:@"ps_main"];
                    _state.fragment_shader = res.func;
                    break;
                case PEN_SHADER_TYPE_CS:
                    if (!res.func)
                        res.func = [res.lib newFunctionWithName:@"cs_main"];
                    _state.compute_shader = res.func;
                    break;
                case PEN_SHADER_TYPE_SO:
                    if (!res.func)
                        res.func = [res.lib newFunctionWithName:@"vs_main"];
                    _state.vertex_shader = res.func;
                    _state.compute_shader = nil;
                    _state.fragment_shader = nil;
                    _state.stream_out = true;
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
            id<MTLBuffer> buf[NBB];

            u32 options = 0;
            u32 num_bufs = 1;
            
            if (params.cpu_access_flags & PEN_CPU_ACCESS_READ)
            {
                options |= MTLResourceOptionCPUCacheModeDefault;
            }
            else if (params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
            {
                options |= MTLResourceCPUCacheModeWriteCombined;
                num_bufs = NBB;
            }

            for (u32 i = 0; i < num_bufs; ++i)
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

            dynamic_buffer& db = _res_pool.get(resource_slot).buffer;
            db.init(&buf[0], num_bufs, params.buffer_size, options, params.bind_flags);

            _res_pool[resource_slot].type = RESOURCE_BUFFER;
        }

        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                         const u32* offsets)
        {
            validate_render_encoder();
            
            static MTLVertexBufferLayoutDescriptor* s_layouts[4] = {
                nil
            };

            for (u32 i = 0; i < num_buffers; ++i)
            {
                u32 ri = buffer_indices[i];
                u32 stride = strides[i];
                
                auto& bb = _res_pool.get(ri).buffer;

                [_state.render_encoder setVertexBuffer:bb.read()
                                                offset:offsets[i]
                                               atIndex:start_slot + i];

                MTLVertexBufferLayoutDescriptor* layout = [MTLVertexBufferLayoutDescriptor new];

                layout.stride = stride;
                layout.stepFunction = i == 0 ? MTLVertexStepFunctionPerVertex : MTLVertexStepFunctionPerInstance;
                layout.stepRate = 1;

                [_state.vertex_descriptor.layouts setObject:layout atIndexedSubscript:start_slot + i];
                
                u32 si = start_slot + i;
                if(s_layouts[si])
                {
                    [s_layouts[si] release];
                    s_layouts[si] = nil;
                }
                
                s_layouts[si] = layout;
            }
        }

        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
        {
            index_buffer_cmd& ib = _state.index_buffer;
            ib.buffer = _res_pool.get(buffer_index).buffer.read();
            ib.type = to_metal_index_format(format);
            ib.offset = offset;
            ib.size_bytes = index_size_bytes(format);
        }
        
        inline void _set_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            if(buffer_index == 0)
                return;
            
            size_t bind_offset = 0;
            id<MTLBuffer> buf = _res_pool.get(buffer_index).buffer.read(bind_offset);

            if (flags & pen::CBUFFER_BIND_VS)
            {
                validate_render_encoder();
                [_state.render_encoder setVertexBuffer:buf
                                                offset:bind_offset
                                               atIndex:resource_slot + CBUF_OFFSET];
            }

            if (flags & pen::CBUFFER_BIND_PS)
            {
                validate_render_encoder();
                [_state.render_encoder setFragmentBuffer:buf
                                                  offset:bind_offset
                                                 atIndex:resource_slot + CBUF_OFFSET];
            }

            // compute command encoder
            if (flags & pen::CBUFFER_BIND_CS)
            {
                validate_compute_encoder();
                [_state.compute_encoder setBuffer:buf
                                           offset:bind_offset
                                          atIndex:resource_slot + CBUF_OFFSET];
            }
        }
        
        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            _set_buffer(buffer_index, resource_slot, flags);
        }
        
        void renderer_set_structured_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            _set_buffer(buffer_index, resource_slot, flags);
        }

        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
        {
            resource& r = _res_pool.get(buffer_index);

            r.buffer.update(data, data_size, offset);
        }

        pen_inline texture_resource create_texture_internal(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
            // resolve backbuffer ratio dimensions and track if necessary
            texture_creation_params _tcp = _renderer_tcp_resolve_ratio(tcp);
            if (track)
                _renderer_track_managed_render_target(tcp, resource_slot);

            if (tcp.num_mips == -1)
            {
                _tcp.num_mips = calc_num_mips(_tcp.width, _tcp.height);
            }

            MTLTextureDescriptor* td = nil;
            id<MTLTexture>        texture = nil;
            id<MTLTexture>        texture_msaa = nil;

            MTLPixelFormat fmt = to_metal_pixel_format(tcp.format);

            u32 num_slices = 1;
            u32 num_arrays = 1;

            bool msaa = false;
            if (_tcp.sample_count > 1)
                msaa = true;

            if (tcp.collection_type == TEXTURE_COLLECTION_NONE || tcp.collection_type == TEXTURE_COLLECTION_ARRAY)
            {
                td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                        width:_tcp.width
                                                                       height:_tcp.height
                                                                    mipmapped:_tcp.num_mips > 1];

                if (tcp.collection_type == TEXTURE_COLLECTION_ARRAY)
                {
                    td.textureType = MTLTextureType2DArray;
                    num_arrays = _tcp.num_arrays;
                    td.arrayLength = num_arrays;

#if 0 //enable if you want MTLTextureType2DMultisampleArray disabled for travis ci in a rush!
                    if (msaa)
                        td.textureType = MTLTextureType2DMultisampleArray;
#else
                    if (msaa)
                    {
                        PEN_ASSERT(0);
                        _tcp.sample_count = 1;
                    }
#endif
                }
                else if (msaa)
                {
                    td.textureType = MTLTextureType2DMultisample;
                }
            }
            else if (tcp.collection_type == TEXTURE_COLLECTION_CUBE)
            {
                td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:fmt
                                                                           size:_tcp.width
                                                                      mipmapped:_tcp.num_mips > 1];

                num_arrays = _tcp.num_arrays;
            }
            else if (tcp.collection_type == TEXTURE_COLLECTION_VOLUME)
            {
                td = [[MTLTextureDescriptor alloc] init];

                td.pixelFormat = fmt;
                td.width = _tcp.width;
                td.height = _tcp.height;
                td.depth = _tcp.num_arrays;
                td.textureType = MTLTextureType3D;
                td.mipmapLevelCount = _tcp.num_mips;
                td.arrayLength = 1;
                td.sampleCount = _tcp.sample_count;

                // arrays become slices
                num_slices = (u32)td.depth;
            }
            else if (tcp.collection_type == TEXTURE_COLLECTION_CUBE_ARRAY)
            {
                td = [[MTLTextureDescriptor alloc] init];

                td.pixelFormat = fmt;
                td.width = _tcp.width;
                td.height = _tcp.height;
                td.depth = 1;
                td.textureType = MTLTextureTypeCubeArray;
                td.mipmapLevelCount = _tcp.num_mips;
                td.arrayLength = _tcp.num_arrays;
                td.sampleCount = _tcp.sample_count;

                num_slices = _tcp.num_arrays;
            }

            if (msaa)
            {
                td.sampleCount = _tcp.sample_count;
            }

            td.usage = to_metal_texture_usage(_tcp.bind_flags);
            td.storageMode = to_metal_storage_mode(_tcp);

            texture = [_metal_device newTextureWithDescriptor:td];

            if (tcp.data)
            {
                u8* mip_data = (u8*)tcp.data;

                for (u32 a = 0; a < num_arrays; ++a)
                {
                    u32 mip_w = tcp.width;
                    u32 mip_h = tcp.height;
                    u32 mip_d = num_slices;

                    for (u32 i = 0; i < tcp.num_mips; ++i)
                    {
                        u32 pitch = _tcp.block_size * (mip_w / tcp.pixels_per_block);
                        u32 depth_pitch = pitch * (mip_h / tcp.pixels_per_block);

                        MTLRegion region;

                        region = MTLRegionMake3D(0, 0, 0, mip_w, mip_h, mip_d);

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
                        mip_d /= 2;

                        mip_w = max<u32>(1, mip_w);
                        mip_h = max<u32>(1, mip_h);
                        mip_d = max<u32>(1, mip_d);
                    }
                }
            }

            MTLPixelFormat fmt_msaa = MTLPixelFormatInvalid;
            if (msaa)
            {
                fmt_msaa = fmt;
                texture_msaa = texture;
                texture = nil;
                msaa = true;
            }

            texture_resource tr;
            tr = {texture, texture_msaa, fmt, fmt_msaa, _tcp.sample_count, tcp, (u32)_tcp.num_mips, 0};

            return tr;
        }

        pen_inline void create_texture(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
            _res_pool.insert(resource(), resource_slot);
            _res_pool.get(resource_slot).texture = create_texture_internal(tcp, resource_slot, track);
        }

        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
        {
            create_texture(tcp, resource_slot, false);
            _res_pool[resource_slot].type = RESOURCE_TEXTURE;
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
            if (texture_index == 0)
                return;

            id<MTLTexture>    tex = _res_pool.get(texture_index).texture.tex;
            if (bind_flags & pen::TEXTURE_BIND_MSAA)
                tex = _res_pool.get(texture_index).texture.tex_msaa;

            if (bind_flags & pen::TEXTURE_BIND_PS || bind_flags & pen::TEXTURE_BIND_VS)
            {
                // render encoder
                validate_render_encoder();
                PEN_ASSERT(_state.render_encoder);

                if (bind_flags & pen::TEXTURE_BIND_PS)
                {
                    [_state.render_encoder setFragmentTexture:tex atIndex:resource_slot];
                    [_state.render_encoder setFragmentSamplerState:_res_pool.get(sampler_index).sampler
                                                           atIndex:resource_slot];
                }

                if (bind_flags & pen::TEXTURE_BIND_VS)
                {
                    [_state.render_encoder setVertexTexture:tex atIndex:resource_slot];
                    [_state.render_encoder setVertexSamplerState:_res_pool.get(sampler_index).sampler atIndex:resource_slot];
                }
            }
            else if (bind_flags & pen::TEXTURE_BIND_CS)
            {
                validate_compute_encoder();
                [_state.compute_encoder setTexture:_res_pool.get(texture_index).texture.tex atIndex:resource_slot];
            }
        }

        void renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp, u32 resource_slot)
        {
            _res_pool.insert(resource(), resource_slot);
            metal_raster_state& rs = _res_pool.get(resource_slot).raster_state;

            rs.cull_mode = to_metal_cull_mode(rscp.cull_mode);
            rs.fill_mode = to_metal_fill_mode(rscp.fill_mode);
            rs.winding = to_metal_winding(rscp.front_ccw);
            rs.scissor_enabled = rscp.scissor_enable == 1;
        }

        void renderer_set_rasterizer_state(u32 rasterizer_state_index)
        {
            _state.raster_state = rasterizer_state_index;
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

            blend.alpha_to_coverage_enable = bcp.alpha_to_coverage_enable;
            blend.num_render_targets = bcp.num_render_targets;

            for (u32 i = 0; i < blend.num_render_targets; ++i)
            {
                blend.attachment[i].enabled = bcp.render_targets[i].blend_enable;
                blend.attachment[i].write_mask = 0;

                // swizzle mask rgba to abgr
                u32 wm = bcp.render_targets[i].render_target_write_mask;
                for (u32 a = 0, b = 3; a < 4; ++a, --b)
                    if (wm & (1 << a))
                        blend.attachment[i].write_mask |= (1 << b);

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

            // stencil
            if (dscp.stencil_enable)
            {
                [dsd.backFaceStencil setStencilFailureOperation:to_metal_stencil_op(dscp.back_face.stencil_failop)];
                [dsd.backFaceStencil setDepthFailureOperation:to_metal_stencil_op(dscp.back_face.stencil_depth_failop)];
                [dsd.backFaceStencil setDepthStencilPassOperation:to_metal_stencil_op(dscp.back_face.stencil_passop)];
                [dsd.backFaceStencil setStencilCompareFunction:to_metal_compare_function(dscp.back_face.stencil_func)];
                [dsd.backFaceStencil setWriteMask:dscp.stencil_write_mask];
                [dsd.backFaceStencil setReadMask:dscp.stencil_read_mask];

                [dsd.frontFaceStencil setStencilFailureOperation:to_metal_stencil_op(dscp.front_face.stencil_failop)];
                [dsd.frontFaceStencil setDepthFailureOperation:to_metal_stencil_op(dscp.front_face.stencil_depth_failop)];
                [dsd.frontFaceStencil setDepthStencilPassOperation:to_metal_stencil_op(dscp.front_face.stencil_passop)];
                [dsd.frontFaceStencil setStencilCompareFunction:to_metal_compare_function(dscp.front_face.stencil_func)];
                [dsd.frontFaceStencil setWriteMask:dscp.stencil_write_mask];
                [dsd.frontFaceStencil setReadMask:dscp.stencil_read_mask];
            }

            _res_pool.get(resource_slot).depth_stencil = [_metal_device newDepthStencilStateWithDescriptor:dsd];
        }

        void renderer_set_depth_stencil_state(u32 depth_stencil_state)
        {
            _state.depth_stencil = _res_pool.get(depth_stencil_state).depth_stencil;
        }

        void renderer_set_stencil_ref(u8 ref)
        {
            _state.stencil_ref = ref;
        }

        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
        {
            validate_render_encoder();
            bind_render_pipeline();

            // draw calls
            [_state.render_encoder drawPrimitives:to_metal_primitive_type(primitive_topology)
                                      vertexStart:start_vertex
                                      vertexCount:vertex_count];
        }

        static pen_inline void _indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                                  u32 base_vertex, u32 primitive_topology)
        {
            validate_render_encoder();
            bind_render_pipeline();

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

        void renderer_dispatch_compute(uint3 grid, uint3 num_threads)
        {
            validate_compute_encoder();
            bind_compute_pipeline();

            MTLSize tgs = MTLSizeMake(num_threads.x, num_threads.y, num_threads.z);

            MTLSize tgc;
            tgc.width = (grid.x + num_threads.x - 1) / num_threads.x;
            tgc.height = (grid.y + num_threads.y - 1) / num_threads.y;
            tgc.depth = (grid.z + num_threads.z - 1) / num_threads.z;

            [_state.compute_encoder dispatchThreadgroups:tgc threadsPerThreadgroup:tgs];

            if (_state.compute_encoder)
            {
                [_state.compute_encoder endEncoding];
                _state.compute_encoder = nil;
            }
        }

        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
            create_texture(tcp, resource_slot, track);
            _res_pool[resource_slot].type = RESOURCE_RENDER_TARGET;
        }

        void renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_slice,
                                  u32 depth_slice)
        {
            if(_state.colour_targets)
            {
                sb_free(_state.colour_targets);
                _state.colour_targets = nullptr;
            }
                
            for(u32 i = 0; i < num_colour_targets; ++i)
                sb_push(_state.colour_targets, colour_targets[i]);
                
            _state.depth_target = depth_target;
            _state.colour_slice = colour_slice;
            _state.depth_slice =  depth_slice;
        }

        void renderer_set_resolve_targets(u32 colour_target, u32 depth_target)
        {
            resource&      r = _res_pool.get(colour_target);
            id<MTLTexture> texture = r.texture.tex;

            // create new cmd buffer
            if (_state.cmd_buffer == nil)
                _state.cmd_buffer = [_state.command_queue commandBuffer];

            // finish render encoding
            if (_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
                _state.pipeline_hash = 0;
            }

            _state.pass = [MTLRenderPassDescriptor renderPassDescriptor];
            _state.target_hash = 0;

            _state.formats.sample_count = 1;
            _state.formats.num_targets = 1;
            _state.pass.colorAttachments[0].texture = texture;
            _state.pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            _state.pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            _state.formats.colour_attachments[0] = r.texture.resolve_fmt;
            _state.formats.depth_attachment = MTLPixelFormatInvalid;
            
            sb_free(_state.colour_targets);
            _state.colour_targets = nullptr;
            _state.depth_target = -1;
        }

        void renderer_resolve_target(u32 target, e_msaa_resolve_type type, resolve_resources res)
        {
            resource&         res_target = _res_pool.get(target);
            texture_resource& t = res_target.texture;
            
            if(type == RESOLVE_GENERATE_MIPS)
            {
                texture_resource& t = _res_pool.get(target).texture;
                
                if (t.num_mips <= 1)
                {
                    PEN_LOG("[error] renderer : render target %i does not have mip maps", target);
                    return;
                }
                
                if (t.num_mips > 1 && t.invalidate)
                {
                    t.invalidate = 0;

                    [_state.render_encoder endEncoding];
                    _state.render_encoder = nil;
                    _state.pipeline_hash = 0;

                    validate_blit_encoder();

                    [_state.blit_encoder generateMipmapsForTexture:t.tex];
                    [_state.blit_encoder endEncoding];
                    _state.blit_encoder = nil;
                }
                return;
            }

            if (!t.tex_msaa)
            {
                PEN_LOG("[error] renderer : render target %i is not an msaa target\n", target);
                return;
            }

            f32 w = t.tcp.width;
            f32 h = t.tcp.height;

            if (t.tcp.width == -1)
            {
                w = pen_window.width / h;
                h = pen_window.height / h;
            }

            // create resolve surface if required
            if (!t.tex)
            {
                // create a resolve surface
                texture_creation_params resolve_tcp = t.tcp;
                resolve_tcp.sample_count = 1;

                texture_creation_params& _tcp = resolve_tcp;
                _tcp.width = w;
                _tcp.height = h;

                // depth gets resolved into colour textures
                if (resolve_tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
                {
                    resolve_tcp.bind_flags &= PEN_BIND_DEPTH_STENCIL;
                    resolve_tcp.bind_flags |= PEN_BIND_RENDER_TARGET;

                    resolve_tcp.format = PEN_TEX_FORMAT_R32_FLOAT;
                }

                resolve_tcp.bind_flags |= PEN_BIND_SHADER_RESOURCE;
                texture_resource resolved = create_texture_internal(resolve_tcp, target, false);
                res_target.texture.tex = resolved.tex;
                res_target.texture.resolve_fmt = resolved.fmt;
            }

            if (type == RESOLVE_CUSTOM)
            {
                resolve_cbuffer cbuf = {w, h, 0.0f, 0.0f};

                direct::renderer_set_resolve_targets(target, 0);

                direct::renderer_update_buffer(res.constant_buffer, &cbuf, sizeof(cbuf), 0);
                direct::renderer_set_constant_buffer(res.constant_buffer, 0, pen::CBUFFER_BIND_PS);

                pen::viewport vp = {0.0f, 0.0f, w, h, 0.0f, 1.0f};
                direct::renderer_set_viewport(vp);

                u32 stride = 24;
                u32 offset = 0;
                direct::renderer_set_vertex_buffers(&res.vertex_buffer, 1, 0, &stride, &offset);
                direct::renderer_set_index_buffer(res.index_buffer, PEN_FORMAT_R16_UINT, 0);

                direct::renderer_set_texture(target, 0, 0, pen::TEXTURE_BIND_MSAA | pen::TEXTURE_BIND_PS);

                direct::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
            }
            else
            {
                if (t.tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
                {
                    PEN_LOG("[error] renderer : render target %i cannot be resolved as it is a depth target\n", target);
                    return;
                }
            }
        }

        void renderer_set_stream_out_target(u32 buffer_index)
        {
            resource& ri = _res_pool.get(buffer_index);

            u32 bi = PEN_BACK_BUFFER_COLOUR;
            renderer_set_targets(&bi, 1, 0);

            validate_render_encoder();

            [_state.render_encoder setVertexBuffer:ri.buffer.read() offset:0 atIndex:7];
        }

        void renderer_read_back_resource(const resource_read_back_params& rrbp)
        {
            if (_state.cmd_buffer == nil)
                _state.cmd_buffer = [_state.command_queue commandBuffer];

            if (rrbp.resource_index == 0)
            {
                // todo backbuffer
            }
            else
            {
                resource& res = _res_pool[rrbp.resource_index];
                if (res.type == RESOURCE_TEXTURE || res.type == RESOURCE_RENDER_TARGET)
                {
                    texture_resource& tr = res.texture;

                    id<MTLBuffer> stage =
                        [_metal_device newBufferWithLength:(rrbp.data_size)options:MTLResourceOptionCPUCacheModeDefault];

                    id<MTLBlitCommandEncoder> bce = [_state.cmd_buffer blitCommandEncoder];

                    u32 w = tr.tcp.width;
                    u32 h = tr.tcp.height;

                    if (w == -1)
                    {
                        w = pen_window.width / h;
                        h = pen_window.height / h;
                    }

                    [bce copyFromTexture:tr.tex
                                     sourceSlice:0
                                     sourceLevel:0
                                    sourceOrigin:MTLOriginMake(0, 0, 0)
                                      sourceSize:MTLSizeMake(w, h, 1)
                                        toBuffer:stage
                               destinationOffset:0
                          destinationBytesPerRow:rrbp.row_pitch
                        destinationBytesPerImage:rrbp.depth_pitch];

                    [bce endEncoding];

                    [_state.cmd_buffer commit];
                    [_state.cmd_buffer waitUntilCompleted];
                    _state.cmd_buffer = nil;

                    rrbp.call_back_function([stage contents], rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size);
                }
            }
        }
        
        void renderer_present()
        {
            if (_state.render_encoder)
            {
                [_state.render_encoder endEncoding];
                _state.render_encoder = nil;
            }
            
            _renderer_commit_stretchy_dynamic_buffers();

            // flush cmd buf and present
            [_state.cmd_buffer presentDrawable:_metal_view.currentDrawable];
            _state.drawable = nil;
            _state.target_hash = 0;
            
            static std::atomic<u32> waits = { 0 };
            waits++;
            
            [_state.cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
                waits--;
            }];

            [_state.cmd_buffer commit];
            
            while(waits == NBB)
                thread_sleep_us(100);

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
            [_res_pool.get(shader_index).shader.lib release];
            _res_pool.get(shader_index).shader.lib = nil;
        }

        void renderer_release_clear_state(u32 clear_state)
        {
            // no allocs
        }

        void renderer_release_buffer(u32 buffer_index)
        {
            for (u32 i = 0; i < NBB; ++i)
                _res_pool.get(buffer_index).buffer.release();
        }

        void renderer_release_texture(u32 texture_index)
        {
            auto tex = _res_pool.get(texture_index).texture.tex;
            [tex setPurgeableState:MTLPurgeableStateEmpty];
            [tex release];
            tex = nil;
        }

        void renderer_release_sampler(u32 sampler)
        {
            auto samp = _res_pool.get(sampler).sampler;
            [samp release];
            samp = nil;
        }

        void renderer_release_raster_state(u32 raster_state_index)
        {
        }

        void renderer_release_blend_state(u32 blend_state)
        {
        }

        void renderer_release_render_target(u32 render_target)
        {
            renderer_release_texture(render_target);
        }

        void renderer_release_input_layout(u32 input_layout)
        {
            auto vd = _res_pool.get(input_layout).vertex_descriptor;
            [vd release];
            vd = nil;
        }

        void renderer_release_depth_stencil_state(u32 depth_stencil_state)
        {
        }
    }
}

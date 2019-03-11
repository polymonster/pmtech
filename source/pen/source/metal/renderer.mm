// renderer.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "console.h"
#include "data_struct.h"
#include "renderer.h"
#include "str/Str.h"

// globals
a_u8 g_window_resize;

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
        u32           size_bytes;
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
        MTLScissorRect       scissor;
        MTLVertexDescriptor* vertex_descriptor;
        id<MTLFunction>      vertex_shader;
        id<MTLFunction>      fragment_shader;
        index_buffer_cmd     index_buffer;
        u32                  input_layout;
        u32                  blend_state;
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
            id<MTLBuffer>        buffer;
            texture_resource     texture;
            id<MTLSamplerState>  sampler;
            shader_resource      shader;
            metal_clear_state    clear;
            MTLVertexDescriptor* vertex_descriptor;
            metal_blend_state    blend;
        };
    };

    res_pool<resource> _res_pool;
    MTKView*           _metal_view;
    id<MTLDevice>      _metal_device;
    current_state      _state;
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
        }

        void bind_pipeline()
        {
            // todo cache this

            // create pipeline
            MTLRenderPipelineDescriptor* pipeline_desc = [MTLRenderPipelineDescriptor new];

            pipeline_desc.vertexFunction = _state.vertex_shader;
            pipeline_desc.fragmentFunction = _state.fragment_shader;
            pipeline_desc.colorAttachments[0].pixelFormat = _state.formats.colour_attachments[0];

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

            return 1;
        }

        void renderer_shutdown()
        {
            // nothing to do yet
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
                    NSLog(@" error => %@ ", err);
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
                u32 stride = strides[i];

                [_state.render_encoder setVertexBuffer:_res_pool.get(ri).buffer offset:offsets[i] atIndex:start_slot + i];

                MTLVertexBufferLayoutDescriptor* layout = [MTLVertexBufferLayoutDescriptor new];
                layout.stride = stride;
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
            ib.offset = offset;
            ib.size_bytes = index_size_bytes(format);
        }

        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            validate_encoder();

            // todo ps

            [_state.render_encoder setVertexBuffer:_res_pool.get(buffer_index).buffer offset:0 atIndex:resource_slot + 8];
        }

        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
        {
            u8* pdata = (u8*)[_res_pool.get(buffer_index).buffer contents];
            pdata = pdata + offset;

            memcpy(pdata, data, data_size);
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

                    for (u32 i = 0; i < tcp.num_mips; ++i)
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
            // todo..
        }

        void renderer_set_depth_stencil_state(u32 depth_stencil_state)
        {
            // todo..
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

            if (num_colour_targets == 1 && colour_targets[0] == PEN_BACK_BUFFER_COLOUR)
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
                for (u32 i = 0; i < num_colour_targets; ++i)
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

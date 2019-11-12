// renderer_vulkan.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "renderer.h"
#include "renderer_shared.h"
#include "console.h"
#include "data_struct.h"
#include "hash.h"


#include "vulkan/vulkan.h"
#ifdef _WIN32
#include "vulkan/vulkan_win32.h"
#endif

#define CHECK_CALL(C) { VkResult r = (C); PEN_ASSERT(r == VK_SUCCESS); }

extern pen::window_creation_params pen_window;
a_u8                               g_window_resize(0);

using namespace pen;

#define NBB 3 // num "back buffers" / swap chains / inflight command buffers

namespace
{
    // conversion functions
    VkBufferUsageFlags to_vk_buffer_usage(u32 pen_bind_flags)
    {
        switch (pen_bind_flags)
        {
        case PEN_BIND_VERTEX_BUFFER:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case PEN_BIND_INDEX_BUFFER:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case PEN_BIND_CONSTANT_BUFFER:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        PEN_ASSERT(0);
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    VkPolygonMode to_vk_polygon_mode(u32 pen_polygon_mode)
    {
        switch (pen_polygon_mode)
        {
        case PEN_FILL_SOLID:
            return VK_POLYGON_MODE_FILL;
        case PEN_FILL_WIREFRAME:
            return VK_POLYGON_MODE_LINE;
        }
        PEN_ASSERT(0);
        return VK_POLYGON_MODE_FILL;
    }

    VkCullModeFlags to_vk_cull_mode(u32 pen_cull_mode)
    {
        switch (pen_cull_mode)
        {
        case PEN_CULL_NONE:
            return VK_CULL_MODE_NONE;
        case PEN_CULL_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case PEN_CULL_BACK:
            return VK_CULL_MODE_BACK_BIT;
        }
        PEN_ASSERT(0);
        return VK_CULL_MODE_NONE;
    }

    VkPrimitiveTopology to_vk_primitive_topology(u32 pen_primitive_topology)
    {
        switch (pen_primitive_topology)
        {
        case PEN_PT_POINTLIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PEN_PT_LINELIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PEN_PT_LINESTRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PEN_PT_TRIANGLELIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PEN_PT_TRIANGLESTRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        }
        PEN_ASSERT(0);
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    VkFormat to_vk_vertex_format(u32 pen_vertex_format)
    {
        switch (pen_vertex_format)
        {
        case PEN_VERTEX_FORMAT_FLOAT1:
            return VK_FORMAT_R32_SFLOAT;
        case PEN_VERTEX_FORMAT_FLOAT2:
            return VK_FORMAT_R32G32_SFLOAT;
        case PEN_VERTEX_FORMAT_FLOAT3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case PEN_VERTEX_FORMAT_FLOAT4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PEN_VERTEX_FORMAT_UNORM4:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PEN_VERTEX_FORMAT_UNORM2:
            return VK_FORMAT_R8G8_UNORM;
        case PEN_VERTEX_FORMAT_UNORM1:
            return VK_FORMAT_R8_UNORM;
        }
        PEN_ASSERT(0);
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    VkIndexType to_vk_index_type(u32 pen_index_type)
    {
        switch (pen_index_type)
        {
        case PEN_FORMAT_R16_UINT:
            return VK_INDEX_TYPE_UINT16;
        case PEN_FORMAT_R32_UINT:
            return VK_INDEX_TYPE_UINT32;
        }
        PEN_ASSERT(0);
        return VK_INDEX_TYPE_UINT16;
    }

    VkFilter to_vk_filter(u32 pen_filter)
    {
        switch (pen_filter)
        {
        case PEN_FILTER_MIN_MAG_MIP_LINEAR:
        case PEN_FILTER_LINEAR:
            return VK_FILTER_LINEAR;
        case PEN_FILTER_MIN_MAG_MIP_POINT:
        case PEN_FILTER_POINT:
            return VK_FILTER_NEAREST;
        }
        PEN_ASSERT(0);
        return VK_FILTER_LINEAR;
    }

    VkSamplerMipmapMode to_vk_mip_map_mode(u32 pen_filter)
    {
        switch (pen_filter)
        {
        case PEN_FILTER_MIN_MAG_MIP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case PEN_FILTER_MIN_MAG_MIP_POINT:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case PEN_FILTER_POINT:
        case PEN_FILTER_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }
        PEN_ASSERT(0);
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    VkSamplerAddressMode to_vk_sampler_address_mode(u32 pen_sampler_address_mode)
    {
        switch (pen_sampler_address_mode)
        {
        case PEN_TEXTURE_ADDRESS_WRAP:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case PEN_TEXTURE_ADDRESS_MIRROR:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case PEN_TEXTURE_ADDRESS_CLAMP:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case PEN_TEXTURE_ADDRESS_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case PEN_TEXTURE_ADDRESS_MIRROR_ONCE:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        }
        PEN_ASSERT(0);
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    bool is_compressed_tex_format(u32 pen_format)
    {
        switch (pen_format)
        {
        case PEN_TEX_FORMAT_BC1_UNORM:
        case PEN_TEX_FORMAT_BC2_UNORM:
        case PEN_TEX_FORMAT_BC3_UNORM:
        case PEN_TEX_FORMAT_BC4_UNORM:
        case PEN_TEX_FORMAT_BC5_UNORM:
            return true;
        }
        return false;
    }

    bool is_depth_stencil_tex_format(u32 pen_format)
    {
        switch (pen_format)
        {
        case PEN_TEX_FORMAT_D24_UNORM_S8_UINT:
            return true;
        }
        return false;
    }

    VkImageAspectFlags to_vk_image_aspect(u32 pen_texture_format)
    {
        if (pen_texture_format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageUsageFlags to_vk_texture_usage(u32 pen_texture_usage, u32 pen_texture_format, bool has_data)
    {
        u32 vf = 0;
        if (pen_texture_usage & PEN_BIND_DEPTH_STENCIL)
        {
            vf |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            vf |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (!is_compressed_tex_format(pen_texture_format))
                vf |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (pen_texture_usage & PEN_BIND_RENDER_TARGET)
                vf |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if (has_data)
            vf |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return (VkImageUsageFlagBits)vf;
    }

    VkShaderStageFlags to_vk_stage(u32 pen_bind_flags)
    {
        u32 ss = 0;
        if (pen_bind_flags & pen::TEXTURE_BIND_VS || pen_bind_flags & pen::CBUFFER_BIND_VS)
            ss |= VK_SHADER_STAGE_VERTEX_BIT;
        if (pen_bind_flags & pen::TEXTURE_BIND_PS || pen_bind_flags & pen::CBUFFER_BIND_PS)
            ss |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (pen_bind_flags & pen::TEXTURE_BIND_CS)
            ss |= VK_SHADER_STAGE_COMPUTE_BIT;
        return (VkShaderStageFlags)ss;
    }

    VkBlendOp to_vk_blend_op(u32 pen_blend_op)
    {
        switch (pen_blend_op)
        {
        case PEN_BLEND_OP_ADD:
            return VK_BLEND_OP_ADD;
        case PEN_BLEND_OP_SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case PEN_BLEND_OP_REV_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case PEN_BLEND_OP_MIN:
            return VK_BLEND_OP_MIN;
        case PEN_BLEND_OP_MAX:
            return VK_BLEND_OP_MAX;
        }
        PEN_ASSERT(0);
        return VK_BLEND_OP_ADD;
    }

    VkBlendFactor to_vk_blend_factor(u32 pen_blend_factor)
    {
        switch (pen_blend_factor)
        {
        case PEN_BLEND_ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case PEN_BLEND_ONE:
            return VK_BLEND_FACTOR_ONE;
        case PEN_BLEND_SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case PEN_BLEND_INV_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case PEN_BLEND_SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case PEN_BLEND_INV_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case PEN_BLEND_DEST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case PEN_BLEND_INV_DEST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case PEN_BLEND_INV_DEST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case PEN_BLEND_SRC_ALPHA_SAT:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case PEN_BLEND_SRC1_COLOR:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case PEN_BLEND_INV_SRC1_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case PEN_BLEND_SRC1_ALPHA:
            return VK_BLEND_FACTOR_SRC1_ALPHA;
        case PEN_BLEND_INV_SRC1_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        case PEN_BLEND_BLEND_FACTOR:
        case PEN_BLEND_INV_BLEND_FACTOR:
            PEN_ASSERT(0);
            break;
        }
        PEN_ASSERT(0);
        return VK_BLEND_FACTOR_ZERO;
    }

    VkFormat to_vk_image_format(u32 pen_image_format)
    {
        switch (pen_image_format)
        {
        case PEN_TEX_FORMAT_RGBA8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PEN_TEX_FORMAT_BGRA8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case PEN_TEX_FORMAT_R32G32B32A32_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PEN_TEX_FORMAT_R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case PEN_TEX_FORMAT_R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case PEN_TEX_FORMAT_R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PEN_TEX_FORMAT_R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case PEN_TEX_FORMAT_R32_UINT:
            return VK_FORMAT_R32_UINT;
        case PEN_TEX_FORMAT_R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case PEN_TEX_FORMAT_BC1_UNORM:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case PEN_TEX_FORMAT_BC2_UNORM:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case PEN_TEX_FORMAT_BC3_UNORM:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case PEN_TEX_FORMAT_BC4_UNORM:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case PEN_TEX_FORMAT_BC5_UNORM:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case PEN_TEX_FORMAT_D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
            break;
        }
        // unhandled
        PEN_ASSERT(0);
        return VK_FORMAT_UNDEFINED;
    }

    enum e_submit_flags
    {
        SUBMIT_GRAPHICS = 1 << 0,
        SUBMIT_COMPUTE = 1 << 1
    };

    // vulkan internals
    struct vulkan_context
    {
        VkInstance                          instance;
        const char**                        layer_names = nullptr;
        const char**                        ext_names = nullptr;
        VkDebugUtilsMessengerEXT            debug_messanger;
        bool                                enable_validation = false;
        VkPhysicalDevice                    physical_device = VK_NULL_HANDLE;
        u32                                 num_queue_families;
        u32                                 graphics_family_index;
        VkQueue                             graphics_queue;
        u32                                 present_family_index;
        VkQueue                             present_queue;
        u32                                 compute_family_index;
        VkQueue                             compute_queue;
        VkDevice                            device;
        VkSurfaceKHR                        surface;
        VkSwapchainKHR                      swap_chain = VK_NULL_HANDLE;
        VkImage*                            swap_chain_images = nullptr;
        VkCommandPool                       cmd_pool;
        VkCommandBuffer*                    cmd_bufs = nullptr;
        VkCommandPool                       cmd_pool_compute;
        VkCommandBuffer*                    cmd_buf_compute = nullptr;
        u32                                 ii = 0; // image index 0 - NBB, next = (ii + i) % NBB
        VkSemaphore                         sem_img_avail[NBB];
        VkSemaphore                         sem_render_finished[NBB];
        VkFence                             fences[NBB];
        VkFence                             compute_fences[NBB];
        VkPhysicalDeviceMemoryProperties    mem_properties;
        VkDescriptorPool                    descriptor_pool[NBB];
        u32                                 submit_flags = 0;
    };
    vulkan_context _ctx;

    struct pen_binding
    {
        u32                 stage;
        VkDescriptorType    descriptor_type;
        union
        {
            struct
            {
                u32 index;
                u32 sampler_index;
                u32 slot;
                u32 bind_flags;
            };

            struct
            {
                u32 index;
                u32 slot;
                u32 bind_flags;
            };
        };
    };

    struct vk_pass_cache
    {
        VkRenderPassBeginInfo   begin_info;
        VkRenderPass            pass = VK_NULL_HANDLE;
    };
    hash_id*        s_pass_cache_hash = nullptr;
    vk_pass_cache*  s_pass_cache = nullptr;

    struct vk_pipeline_cache
    {
        VkPipeline              pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout   descriptor_set_layout = VK_NULL_HANDLE;
        VkPipelineLayout        pipeline_layout = VK_NULL_HANDLE;
    };
    hash_id*            s_pipeline_cache_hash = nullptr;
    vk_pipeline_cache*  s_pipeline_cache = nullptr;

    enum e_shd
    {
        vertex,
        fragment,
        geometry,
        stream_out,
        compute,
        count
    };
    static_assert(e_shd::compute == PEN_SHADER_TYPE_CS, "mismatched shader types");

    struct pen_state
    {
        // hashes
        hash_id                             hpass = 0;
        hash_id                             hpipeline = 0;
        hash_id                             hdescriptors = 0;
        // hash for pass
        u32*                                colour_attachments = nullptr;
        u32                                 depth_attachment = 0;
        u32                                 colour_slice;
        u32                                 depth_slice;
        u32                                 clear_state;
        // hash for pipeline
        u32                                 shader[e_shd::count]; // vs, fs, gs, so, cs
        viewport                            vp;
        rect                                sr;
        u32                                 vertex_buffer;
        u32                                 index_buffer;
        u32                                 input_layout;
        u32                                 raster;
        u32                                 blend = -1;
        VkRenderPass                        pass;
        pen_binding*                        bindings = nullptr;
        // vulkan cached state
        VkPipelineLayout                    pipeline_layout;
        VkVertexInputBindingDescription*    vertex_input_bindings = nullptr;
        VkDescriptorSetLayout               descriptor_set_layout;
        u32                                 descriptor_set_index;
        u32                                 pipeline_index = -1;
    };
    pen_state _state;

    struct descriptor_set_alloc
    {
        VkDescriptorSet set;
        u64             frame;
    };
    descriptor_set_alloc* s_descriptor_sets = nullptr;

    struct vulkan_texture
    {
        VkImageView                 image_view;
        VkImage                     image;
        VkFormat                    format;
        VkDeviceMemory              mem = 0;
        texture_creation_params*    tcp;
        VkImageLayout               layout;
        bool                        compute_shader_write = false;
    };

    struct vulkan_buffer
    {
        VkBuffer        buf[NBB];
        VkDeviceMemory  mem[NBB];
        u32             size;
        bool            dynamic;

        VkBuffer& get_buffer()
        {
            if (dynamic)
                return buf[_ctx.ii];

            return buf[0];
        }

        VkDeviceMemory& get_mem()
        {
            if (dynamic)
                return mem[_ctx.ii];

            return mem[0];
        }
    };

    struct vulkan_shader
    {
        VkShaderModule module;
        e_shd          type;
    };

    struct vulkan_blend_state
    {
        VkPipelineColorBlendAttachmentState* attachments;
        VkPipelineColorBlendStateCreateInfo  info;
    };

    enum class e_res
    {
        none,
        clear,
        texture,
        render_target,
        shader,
    };

    struct resource_allocation
    {
        e_res type = e_res::none;

        union {
            vulkan_texture                          texture;
            vulkan_shader                           shader;
            clear_state                             clear;
            vulkan_buffer                           buffer;
            VkPipelineRasterizationStateCreateInfo  raster;
            VkVertexInputAttributeDescription*      vertex_attributes;
            VkSampler                               sampler;
            vulkan_blend_state                      blend;
        };
    };
    res_pool<resource_allocation> _res_pool;

    // hash contents of a stretchy buffer
    template<typename T>
    hash_id sb_hash(T* sb)
    {
        u32 c = sb_count(sb);
        HashMurmur2A hh;
        hh.begin();
        for (u32 i = 0; i < c; ++i)
            hh.add(sb[i]);
        return hh.end();
    }

    void destroy_caches()
    {
        // render passes
        u32 pc = sb_count(s_pass_cache);
        for (u32 i = 0; i < pc; ++i)
        {
            vkDestroyRenderPass(_ctx.device, s_pass_cache[i].pass, nullptr);
            delete s_pass_cache[i].begin_info.pClearValues;
        }

        sb_free(s_pass_cache);
        sb_free(s_pass_cache_hash);
        s_pass_cache = nullptr;
        s_pass_cache_hash = nullptr;

        // pipelines
        pc = sb_count(s_pipeline_cache);
        for (u32 i = 0; i < pc; ++i)
        {
            vkDestroyPipeline(_ctx.device, s_pipeline_cache[i].pipeline, nullptr);
            vkDestroyPipelineLayout(_ctx.device, s_pipeline_cache[i].pipeline_layout, nullptr);
            vkDestroyDescriptorSetLayout(_ctx.device, s_pipeline_cache[i].descriptor_set_layout, nullptr);
        }

        sb_free(s_pipeline_cache);
        sb_free(s_pipeline_cache_hash);
        s_pipeline_cache = nullptr;
        s_pipeline_cache_hash = nullptr;
    }

    void destory_swapchain()
    {
        if (_ctx.swap_chain)
        {
            vkDestroySwapchainKHR(_ctx.device, _ctx.swap_chain, nullptr);
            _ctx.swap_chain = VK_NULL_HANDLE;
        }

        u32 st = sb_count(_ctx.swap_chain_images);
        for (u32 i = 0; i < st; ++i)
            vkDestroyImageView(_ctx.device, _res_pool.get(i).texture.image_view, nullptr);

        sb_free(_ctx.swap_chain_images);
        _ctx.swap_chain_images = nullptr;
    }

    void enumerate_layers()
    {
        u32 num_layer_props;
        vkEnumerateInstanceLayerProperties(&num_layer_props, nullptr);

        VkLayerProperties* layer_props = new VkLayerProperties[num_layer_props];
        vkEnumerateInstanceLayerProperties(&num_layer_props, layer_props);

        for (u32 i = 0; i < num_layer_props; ++i)
            sb_push(_ctx.layer_names, layer_props[i].layerName);
    }

    void enumerate_extensions()
    {
        u32 num_ext_props;
        vkEnumerateInstanceExtensionProperties(nullptr, &num_ext_props, nullptr);

        VkExtensionProperties* ext_props = new VkExtensionProperties[num_ext_props];
        vkEnumerateInstanceExtensionProperties(nullptr, &num_ext_props, ext_props);

        for (u32 i = 0; i < num_ext_props; ++i)
            sb_push(_ctx.ext_names, ext_props[i].extensionName);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        PEN_LOG("[vulkan validation] %s\n", pCallbackData->pMessage);
        return VK_FALSE;
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debugMessenger, pAllocator);
    }

    void create_debug_messenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = debug_callback;
        info.pUserData = nullptr;

        VkResult res = CreateDebugUtilsMessengerEXT(_ctx.instance, &info, nullptr, &_ctx.debug_messanger);
        PEN_ASSERT(res == VK_SUCCESS);
    }

    void destroy_debug_messenger()
    {
        DestroyDebugUtilsMessengerEXT(_ctx.instance, _ctx.debug_messanger, nullptr);
    }

    void create_surface(void* params)
    {
        // in win32 params is HWND
        HWND hwnd = *((HWND*)params);

        VkWin32SurfaceCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hwnd = hwnd;
        info.hinstance = GetModuleHandle(nullptr);

        CHECK_CALL(vkCreateWin32SurfaceKHR(_ctx.instance, &info, nullptr, &_ctx.surface));
    }

    void create_backbuffer_targets()
    {
        // get swapchain images
        u32 num_images = 0;
        vkGetSwapchainImagesKHR(_ctx.device, _ctx.swap_chain, &num_images, nullptr);

        VkImage* images = new VkImage[num_images];
        vkGetSwapchainImagesKHR(_ctx.device, _ctx.swap_chain, &num_images, images);

        for (u32 i = 0; i < num_images; ++i)
            sb_push(_ctx.swap_chain_images, images[i]);

        for (u32 i = 0; i < num_images; ++i)
        {
            _res_pool.insert({}, i);
            vulkan_texture& vt = _res_pool.get(i).texture;

            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = _ctx.swap_chain_images[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = VK_FORMAT_B8G8R8A8_UNORM;
            info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;

            VkImageView iv;
            CHECK_CALL(vkCreateImageView(_ctx.device, &info, nullptr, &iv));

            vt.image_view = iv;
            vt.image = _ctx.swap_chain_images[i];
            vt.format = VK_FORMAT_B8G8R8A8_UNORM;

            vt.tcp = new texture_creation_params();
            memset(vt.tcp, 0x0, sizeof(vt.tcp));

            vt.tcp->width = pen_window.width;
            vt.tcp->height = pen_window.height;
        }

        // depth images
        for (u32 i = 0; i < num_images; ++i)
        {
            continue;

            u32 j = i + num_images;

            _res_pool.insert({}, j);
            vulkan_texture& vt = _res_pool.get(j).texture;

            pen::texture_creation_params tcp;
            tcp.data = nullptr;
            tcp.data_size = 0;
            tcp.width = pen_window.width;
            tcp.height = pen_window.height;
            tcp.num_arrays = 1;
            tcp.num_mips = 1;
            tcp.sample_count = 1;
            tcp.sample_quality = 1;
            tcp.block_size = 1;
            tcp.pixels_per_block = 1;
            tcp.format = PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
            tcp.bind_flags = PEN_BIND_DEPTH_STENCIL;
            tcp.collection_type = TEXTURE_COLLECTION_NONE;

            direct::renderer_create_texture(tcp, j);
        }

        delete images;
    }

    void create_command_buffers()
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = 0;
        info.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        CHECK_CALL(vkCreateCommandPool(_ctx.device, &info, NULL, &_ctx.cmd_pool));

        VkCommandBufferAllocateInfo buf_info = {};
        buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buf_info.commandPool = _ctx.cmd_pool;
        buf_info.commandBufferCount = NBB;
        buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        // make enough size
        for (u32 i = 0; i < buf_info.commandBufferCount; ++i)
            sb_push(_ctx.cmd_bufs, VkCommandBuffer());

        CHECK_CALL(vkAllocateCommandBuffers(_ctx.device, &buf_info, _ctx.cmd_bufs));

        // allocate command buffer for compute
        VkCommandPoolCreateInfo compute_pool_info = {};
        compute_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        compute_pool_info.queueFamilyIndex = _ctx.compute_family_index;
        compute_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        CHECK_CALL(vkCreateCommandPool(_ctx.device, &compute_pool_info, nullptr, &_ctx.cmd_pool_compute));

        // Create a command buffer for compute operations
        VkCommandBufferAllocateInfo compute_buf_info = {};
        compute_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        compute_buf_info.commandPool = _ctx.cmd_pool_compute;
        compute_buf_info.commandBufferCount = NBB;
        compute_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        // make enough size
        for (u32 i = 0; i < compute_buf_info.commandBufferCount; ++i)
            sb_push(_ctx.cmd_buf_compute, VkCommandBuffer());

        CHECK_CALL(vkAllocateCommandBuffers(_ctx.device, &compute_buf_info, _ctx.cmd_buf_compute));
    }

    void create_descriptor_set_pools(u32 size)
    {
        // one large pool for all descriptor sets to be allocated from

        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , size },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , size },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , size },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, size }
        };

        u32 max_sets = 0;
        for (auto& ps : pool_sizes)
            max_sets += ps.descriptorCount;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = PEN_ARRAY_SIZE(pool_sizes);
        pool_info.pPoolSizes = &pool_sizes[0];
        pool_info.maxSets = max_sets;

        for (u32 i = 0; i < NBB; ++i)
            CHECK_CALL(vkCreateDescriptorPool(_ctx.device, &pool_info, nullptr, &_ctx.descriptor_pool[i]));
    }

    // quick dirty functions for testing, better having a pool of these to burn through
    VkCommandBuffer begin_cmd_buffer()
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandPool = _ctx.cmd_pool;
        info.commandBufferCount = 1;

        VkCommandBuffer cmd_buf;
        CHECK_CALL(vkAllocateCommandBuffers(_ctx.device, &info, &cmd_buf));

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd_buf, &begin_info);

        return cmd_buf;
    }

    void end_cmd_buffer(VkCommandBuffer cmd_buf)
    {
        vkEndCommandBuffer(cmd_buf);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buf;

        vkQueueSubmit(_ctx.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(_ctx.graphics_queue);

        vkFreeCommandBuffers(_ctx.device, _ctx.cmd_pool, 1, &cmd_buf);
    }

    void _transition_image_cs(VkCommandBuffer cmd_buf, VkImage image, VkFormat format, VkImageLayout old_layout,
        VkImageLayout new_layout)
    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = _ctx.compute_family_index;
        barrier.dstQueueFamilyIndex = _ctx.compute_family_index;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags src_stage;
        VkPipelineStageFlags dst_stage;

        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void _transition_image(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
    {
        VkCommandBuffer cmd_buf = begin_cmd_buffer();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = _ctx.graphics_family_index;
        barrier.dstQueueFamilyIndex = _ctx.graphics_family_index;;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags src_stage;
        VkPipelineStageFlags dst_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
            && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
            && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else
        {
            // unsupported transition
            PEN_ASSERT(0);
        }

        vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        end_cmd_buffer(cmd_buf);
    }

    void create_sync_primitives()
    {
        VkSemaphoreCreateInfo sem_info = {};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (u32 i = 0; i < NBB; ++i)
        {
            CHECK_CALL(vkCreateSemaphore(_ctx.device, &sem_info, nullptr, &_ctx.sem_img_avail[i]));
            CHECK_CALL(vkCreateSemaphore(_ctx.device, &sem_info, nullptr, &_ctx.sem_render_finished[i]));
            CHECK_CALL(vkCreateFence(_ctx.device, &fence_info, nullptr, &_ctx.fences[i]));
            CHECK_CALL(vkCreateFence(_ctx.device, &fence_info, nullptr, &_ctx.compute_fences[i]));
        }
    }

    void create_swapchain()
    {
        destroy_caches();
        destory_swapchain();

        VkSurfaceCapabilitiesKHR caps;
        CHECK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_ctx.physical_device, _ctx.surface, &caps));

        pen_window.width = caps.currentExtent.width;
        pen_window.height = caps.currentExtent.height;

        VkSwapchainCreateInfoKHR swap_chain_info = {};
        swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swap_chain_info.surface = _ctx.surface;
        swap_chain_info.minImageCount = NBB;
        swap_chain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swap_chain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swap_chain_info.imageExtent = { pen_window.width, caps.currentExtent.height };
        swap_chain_info.imageArrayLayers = 1;
        swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swap_chain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swap_chain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swap_chain_info.oldSwapchain = _ctx.swap_chain;
        swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swap_chain_info.clipped = VK_TRUE;
        swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        CHECK_CALL(vkCreateSwapchainKHR(_ctx.device, &swap_chain_info, nullptr, &_ctx.swap_chain));

        create_backbuffer_targets();
    }

    void create_device_surface_swapchain(void* params)
    {
        u32 dev_count;
        CHECK_CALL(vkEnumeratePhysicalDevices(_ctx.instance, &dev_count, nullptr));
        PEN_ASSERT(dev_count); // no supported devices

        VkPhysicalDevice* devices = new VkPhysicalDevice[dev_count];
        CHECK_CALL(vkEnumeratePhysicalDevices(_ctx.instance, &dev_count, devices));

        _ctx.physical_device = devices[0];

        // surface
        create_surface(params);
        
        // find queues
        _ctx.num_queue_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(_ctx.physical_device, &_ctx.num_queue_families, nullptr);

        VkQueueFamilyProperties* queue_families = new VkQueueFamilyProperties[_ctx.num_queue_families];
        vkGetPhysicalDeviceQueueFamilyProperties(_ctx.physical_device, &_ctx.num_queue_families, queue_families);

        _ctx.graphics_family_index = -1;
        _ctx.present_family_index = -1;
        _ctx.compute_family_index = -1;
        for (u32 i = 0; i < _ctx.num_queue_families; ++i)
        {
            if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                _ctx.graphics_family_index = i;

            if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                _ctx.compute_family_index = i;

            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(_ctx.physical_device, i, _ctx.surface, &present);
            if (queue_families[i].queueCount > 0 && present)
                _ctx.present_family_index = i;

        }
        PEN_ASSERT(_ctx.graphics_family_index != -1);
        PEN_ASSERT(_ctx.present_family_index != -1);
        PEN_ASSERT(_ctx.compute_family_index != -1);

        // gfx queue
        f32 pri = 1.0f;
        VkDeviceQueueCreateInfo gfx_queue_info = {};
        gfx_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gfx_queue_info.queueFamilyIndex = _ctx.graphics_family_index;
        gfx_queue_info.queueCount = 1;
        gfx_queue_info.pQueuePriorities = &pri;

        // present queue
        VkDeviceQueueCreateInfo present_queue_info = {};
        present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_info.queueFamilyIndex = _ctx.present_family_index;
        present_queue_info.queueCount = 1;
        present_queue_info.pQueuePriorities = &pri;

        // compute queue
        VkDeviceQueueCreateInfo compute_queue_info = {};
        compute_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        compute_queue_info.queueFamilyIndex = _ctx.compute_family_index;
        compute_queue_info.queueCount = 1;
        compute_queue_info.pQueuePriorities = &pri;

        VkDeviceQueueCreateInfo* queues = nullptr;
        sb_push(queues, gfx_queue_info);
        sb_push(queues, present_queue_info);
        // sb_push(queues, compute_queue_info);

        // device
        VkPhysicalDeviceFeatures features = {};
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.pQueueCreateInfos = queues;
        info.queueCreateInfoCount = sb_count(queues);
        info.pEnabledFeatures = &features;

        // validation
        if (_ctx.enable_validation)
        {
            static const char** debug_layer;
            sb_push(debug_layer, "VK_LAYER_KHRONOS_validation");
            info.enabledLayerCount = 1;
            info.ppEnabledLayerNames = debug_layer;
        }

        // enable extensions
        {
            static const char** exts;
            sb_push(exts, "VK_KHR_swapchain");

            info.enabledExtensionCount = sb_count(exts);
            info.ppEnabledExtensionNames = exts;
        }

        CHECK_CALL(vkCreateDevice(_ctx.physical_device, &info, nullptr, &_ctx.device));

        vkGetDeviceQueue(_ctx.device, _ctx.graphics_family_index, 0, &_ctx.graphics_queue);
        vkGetDeviceQueue(_ctx.device, _ctx.present_family_index, 0, &_ctx.present_queue);
        vkGetDeviceQueue(_ctx.device, _ctx.compute_family_index, 0, &_ctx.compute_queue);

        // store these?
        {
            u32 num_formats;
            vkGetPhysicalDeviceSurfaceFormatsKHR(_ctx.physical_device, _ctx.surface, &num_formats, nullptr);
            VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[num_formats];
            vkGetPhysicalDeviceSurfaceFormatsKHR(_ctx.physical_device, _ctx.surface, &num_formats, formats);
            delete formats;

            u32 num_present_modes;
            vkGetPhysicalDeviceSurfacePresentModesKHR(_ctx.physical_device, _ctx.surface, &num_present_modes, nullptr);
            VkPresentModeKHR* present_modes = new VkPresentModeKHR[num_present_modes];
            vkGetPhysicalDeviceSurfacePresentModesKHR(_ctx.physical_device, _ctx.surface, &num_formats, present_modes);
            delete present_modes;
        }

        // to query memory type
        vkGetPhysicalDeviceMemoryProperties(_ctx.physical_device, &_ctx.mem_properties);

        create_command_buffers();

        create_sync_primitives();

        create_swapchain();

        create_descriptor_set_pools(4096);

        delete queue_families;
        delete devices;
    }

    u32 get_mem_type(u32 filter, VkMemoryPropertyFlags properties) 
    {
        for (u32 i = 0; i < _ctx.mem_properties.memoryTypeCount; i++) 
        {
            if ((filter & (1 << i)) && (_ctx.mem_properties.memoryTypes[i].propertyFlags & properties) == properties) 
            {
                return i;
            }
        }

        // cannot find suitable memory type
        PEN_ASSERT(0);
        return 0;
    }

    void end_render_pass()
    {
        // end current pass
        vkCmdEndRenderPass(_ctx.cmd_bufs[_ctx.ii]);
        _state.pass = nullptr;

        // clear hashes
        _state.hpipeline = 0;
        _state.hpass = 0;
        _state.hdescriptors = 0;
    }

    void begin_pass_from_cache(const vk_pass_cache& vk_pc, hash_id hash)
    {
        if (_state.pass == vk_pc.pass)
            return;

        if (_state.pass)
            end_render_pass();

        // begin new pass
        vkCmdBeginRenderPass(_ctx.cmd_bufs[_ctx.ii], &vk_pc.begin_info, VK_SUBPASS_CONTENTS_INLINE);
        _state.pass = vk_pc.pass;
        _state.hpass = hash;
    }

    void bind_dynamic_viewport()
    {
        // viewport / scissor
        viewport& vp = _state.vp;
        rect& sr = _state.sr;
        VkViewport viewport = {};
        viewport.x = vp.x;
        viewport.y = vp.y;
        viewport.width = vp.width;
        viewport.height = vp.height;
        viewport.minDepth = vp.min_depth;
        viewport.maxDepth = vp.max_depth;

        vkCmdSetViewport(_ctx.cmd_bufs[_ctx.ii], 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { (s32)sr.left, (s32)sr.top };
        scissor.extent = { (u32)sr.right - (u32)sr.left, (u32)sr.bottom - (u32)sr.top };

        vkCmdSetScissor(_ctx.cmd_bufs[_ctx.ii], 0, 1, &scissor);
    }

    void bind_pipeline_from_cache(const vk_pipeline_cache& pc, VkPipelineBindPoint bind_point, hash_id hash, u32 index)
    {
        if(bind_point == VK_PIPELINE_BIND_POINT_COMPUTE)
            vkCmdBindPipeline(_ctx.cmd_buf_compute[_ctx.ii], bind_point, pc.pipeline);
        else
            vkCmdBindPipeline(_ctx.cmd_bufs[_ctx.ii], bind_point, pc.pipeline);

        _state.descriptor_set_layout = pc.descriptor_set_layout;
        _state.pipeline_layout = pc.pipeline_layout;
        _state.hpipeline = hash;
        _state.pipeline_index = index;

        bind_dynamic_viewport();
    }

    void begin_pass()
    {
        // hash the pass state and check invalidation
        HashMurmur2A hh;
        hh.begin();
        hh.add(sb_hash(_state.colour_attachments));
        hh.add(&_state.depth_attachment, offsetof(pen_state, shader) - offsetof(pen_state, depth_attachment));
        hash_id ph = hh.end();

        // already bound
        if (ph == _state.hpass)
            return;

        // check in pass hashes
        u32 pcc = sb_count(s_pass_cache);
        for (u32 i = 0; i < pcc; ++i)
        {
            if (s_pass_cache_hash[i] == ph)
            {
                // found exisiting
                begin_pass_from_cache(s_pass_cache[i], ph);
                return;
            }
        }

        // begin building a new pipeline
        const clear_state& clear = _res_pool.get(_state.clear_state).clear;

        // attachments
        VkAttachmentDescription* colour_attachments = nullptr;
        VkAttachmentReference*   colour_refs = nullptr;
        VkImageView*             colour_img_view = nullptr;

        size_t fbw, fbh;
        for (u32 i = 0; i < sb_count(_state.colour_attachments); ++i)
        {
            u32 ica = _state.colour_attachments[i];

            const vulkan_texture& vt = _res_pool.get(ica).texture;

            fbw = vt.tcp->width;
            fbh = vt.tcp->height;

            VkAttachmentDescription col = {};
            col.format = vt.format;
            col.samples = VK_SAMPLE_COUNT_1_BIT;

            if (clear.flags & PEN_CLEAR_COLOUR_BUFFER)
                col.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            else
                col.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

            col.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            col.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            col.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (ica < sb_count(_ctx.swap_chain_images))
                col.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            else
                col.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkAttachmentReference ref = {};
            ref.attachment = i;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            sb_push(colour_attachments, col);
            sb_push(colour_refs, ref);
            sb_push(colour_img_view, vt.image_view);
        }

        // sub pass
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = sb_count(colour_refs);
        subpass.pColorAttachments = colour_refs;

        VkSubpassDependency dep = {};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // pass
        VkRenderPassCreateInfo pass_info = {};
        pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass_info.attachmentCount = sb_count(colour_attachments);
        pass_info.pAttachments = colour_attachments;
        pass_info.subpassCount = 1;
        pass_info.pSubpasses = &subpass;
        pass_info.dependencyCount = 1;
        pass_info.pDependencies = &dep;

        VkRenderPass pass;
        CHECK_CALL(vkCreateRenderPass(_ctx.device, &pass_info, nullptr, &pass));

        // framebuffer
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = pass;
        fb_info.attachmentCount = sb_count(colour_img_view);
        fb_info.pAttachments = colour_img_view;
        fb_info.width = fbw;
        fb_info.height = fbh;
        fb_info.layers = 1;

        VkFramebuffer fb;
        CHECK_CALL(vkCreateFramebuffer(_ctx.device, &fb_info, nullptr, &fb));

        sb_free(colour_attachments);
        sb_free(colour_refs);
        sb_free(colour_img_view);

        // clear
        clear_state& cs = _res_pool.get(_state.clear_state).clear;
        VkClearColorValue clear_colour = { cs.r, cs.g, cs.b, cs.a };

        // add new pass into pass cache
        u32 pc_idx = sb_count(s_pass_cache);
        sb_push(s_pass_cache, vk_pass_cache());
        sb_push(s_pass_cache_hash, ph);
        vk_pass_cache& vk_pc = s_pass_cache[pc_idx];
        vk_pc.pass = pass;

        VkClearValue* clear_value = new VkClearValue();
        clear_value->color = clear_colour;

        vk_pc.begin_info = {};
        vk_pc.begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vk_pc.begin_info.renderPass = pass;
        vk_pc.begin_info.framebuffer = fb;
        vk_pc.begin_info.renderArea.offset = { 0, 0 };
        vk_pc.begin_info.renderArea.extent = { (u32)fbw, (u32)fbh };
        vk_pc.begin_info.clearValueCount = 1;
        vk_pc.begin_info.pClearValues = clear_value;

        begin_pass_from_cache(vk_pc, ph);
    }

    void create_pipeline_layout(VkPipelineLayout& pipeline_layout, VkDescriptorSetLayout& descriptor_set_layout)
    {
        // layout
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pushConstantRangeCount = 0;

        VkDescriptorSetLayoutBinding* vk_bindings = nullptr;

        u32 num_bindings = sb_count(_state.bindings);
        for (u32 i = 0; i < num_bindings; ++i)
        {
            auto& b = _state.bindings[i];

            VkDescriptorSetLayoutBinding vb = {};
            vb.binding = b.slot;
            vb.descriptorCount = 1;
            vb.stageFlags = b.stage;
            vb.pImmutableSamplers = nullptr;
            vb.descriptorType = b.descriptor_type;

            sb_push(vk_bindings, vb);
        }

        if (num_bindings > 0)
        {
            VkDescriptorSetLayoutCreateInfo descriptor_info = {};
            descriptor_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_info.bindingCount = sb_count(vk_bindings);
            descriptor_info.pBindings = vk_bindings;

            CHECK_CALL(vkCreateDescriptorSetLayout(_ctx.device, &descriptor_info, nullptr, &descriptor_set_layout));

            pipeline_layout_info.setLayoutCount = 1;
            pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        }

        CHECK_CALL(vkCreatePipelineLayout(_ctx.device, &pipeline_layout_info, nullptr, &pipeline_layout));
    }

    void bind_render_pipeline(u32 primitive_topology)
    {
        // check for invalidation
        HashMurmur2A hh;
        hh.begin();
        hh.add(&_state.shader[0], e_shd::count * sizeof(u32));
        hh.add(_state.blend);
        hh.add(_state.input_layout);
        hash_id ph = hh.end();

        // already bound
        if (ph == _state.hpipeline)
            return;

        // check in pipeline hashes
        u32 plc = sb_count(s_pipeline_cache);
        for (u32 i = 0; i < plc; ++i)
        {
            if (s_pipeline_cache_hash[i] == ph)
            {
                // found exisiting
                bind_pipeline_from_cache(s_pipeline_cache[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ph, i);
                return;
            }
        }

        // create new pipeline
        VkGraphicsPipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        // dynamic states
        VkDynamicState dynamic_states[] = { 
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_info = {};
        dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_info.pNext = nullptr;
        dynamic_info.pDynamicStates = &dynamic_states[0];
        dynamic_info.dynamicStateCount = PEN_ARRAY_SIZE(dynamic_states);
        info.pDynamicState = &dynamic_info;

        // raster
        info.pRasterizationState = &_res_pool.get(_state.raster).raster;

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = to_vk_primitive_topology(primitive_topology);
        input_assembly.primitiveRestartEnable = VK_FALSE;

        info.pInputAssemblyState = &input_assembly;

        // viewport / scissor
        viewport& vp = _state.vp;
        rect& sr = _state.sr;
        VkViewport viewport = {};
        viewport.x = vp.x;
        viewport.y = vp.y;
        viewport.width = vp.width;
        viewport.height = vp.height;
        viewport.minDepth = vp.min_depth;
        viewport.maxDepth = vp.max_depth;

        VkRect2D scissor = {};
        scissor.offset = { (s32)sr.left, (s32)sr.top };
        scissor.extent = { (u32)sr.right - (u32)sr.left, (u32)sr.bottom - (u32)sr.top };

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        info.pViewportState = &viewportState;

        // shader stages
        u32 vs = _state.shader[e_shd::vertex];
        u32 fs = _state.shader[e_shd::fragment];

        VkPipelineShaderStageCreateInfo vertex_shader_info = {};
        vertex_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_shader_info.module = _res_pool.get(vs).shader.module;
        vertex_shader_info.pName = "main";

        VkPipelineShaderStageCreateInfo fragment_shader_info = {};
        fragment_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_info.module = _res_pool.get(fs).shader.module;
        fragment_shader_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_info, fragment_shader_info };

        info.stageCount = 2;
        info.pStages = shader_stages;

        // vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto& va = _res_pool.get(_state.input_layout).vertex_attributes;
        auto& vb = _state.vertex_input_bindings;
        vertex_input_info.vertexBindingDescriptionCount = sb_count(vb);
        vertex_input_info.vertexAttributeDescriptionCount = sb_count(va);
        vertex_input_info.pVertexBindingDescriptions = vb;
        vertex_input_info.pVertexAttributeDescriptions = va;

        info.pVertexInputState = &vertex_input_info;

        // blending
        if (_state.blend != -1)
        {
            info.pColorBlendState = &_res_pool.get(_state.blend).blend.info;
        }
        else
        {
            // make default
            VkPipelineColorBlendAttachmentState colour_blend_attachment = {};
            colour_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colour_blend_attachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colour_blend = {};
            colour_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colour_blend.logicOpEnable = VK_FALSE;
            colour_blend.logicOp = VK_LOGIC_OP_COPY;
            colour_blend.attachmentCount = 1;
            colour_blend.pAttachments = &colour_blend_attachment;
            colour_blend.blendConstants[0] = 0.0f;
            colour_blend.blendConstants[1] = 0.0f;
            colour_blend.blendConstants[2] = 0.0f;
            colour_blend.blendConstants[3] = 0.0f;

            info.pColorBlendState = &colour_blend;
        }

        // multisample
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        info.pMultisampleState = &multisampling;

        // layout
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        create_pipeline_layout(pipeline_layout, descriptor_set_layout);
        info.layout = pipeline_layout;

        // pass
        info.renderPass = _state.pass;
        info.subpass = 0;
        info.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline pipeline;
        CHECK_CALL(vkCreateGraphicsPipelines(_ctx.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

        vk_pipeline_cache new_pipeline;
        new_pipeline.pipeline = pipeline;
        new_pipeline.pipeline_layout = pipeline_layout;
        new_pipeline.descriptor_set_layout = descriptor_set_layout;

        u32 idx = sb_count(s_pipeline_cache);
        sb_push(s_pipeline_cache_hash, ph);
        sb_push(s_pipeline_cache, new_pipeline);

        bind_pipeline_from_cache(s_pipeline_cache[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, ph, idx);
    }

    void bind_compute_pipeline()
    {
        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.flags = 0;
        
        HashMurmur2A hh;
        hh.begin();
        hh.add(_state.shader[e_shd::compute]);
        hash_id ph = hh.end();

        // already bound
        if (ph == _state.hpipeline)
            return;

        // check in pipeline hashes
        u32 plc = sb_count(s_pipeline_cache);
        for (u32 i = 0; i < plc; ++i)
        {
            if (s_pipeline_cache_hash[i] == ph)
            {
                // found exisiting
                bind_pipeline_from_cache(s_pipeline_cache[i], VK_PIPELINE_BIND_POINT_COMPUTE, ph, i);
                return;
            }
        }


        // layout
        VkPipelineLayout pipeline_layout;
        VkDescriptorSetLayout descriptor_set_layout;
        create_pipeline_layout(pipeline_layout, descriptor_set_layout);
        info.layout = pipeline_layout;

        // shader
        u32 cs = _state.shader[e_shd::compute];
        VkPipelineShaderStageCreateInfo compute_shader_info = {};
        compute_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_info.module = _res_pool.get(cs).shader.module;
        compute_shader_info.pName = "main";
        info.stage = compute_shader_info;

        VkPipeline pipeline;
        CHECK_CALL(vkCreateComputePipelines(_ctx.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

        vk_pipeline_cache new_pipeline;
        new_pipeline.pipeline = pipeline;
        new_pipeline.pipeline_layout = pipeline_layout;
        new_pipeline.descriptor_set_layout = descriptor_set_layout;

        u32 idx = sb_count(s_pipeline_cache);
        sb_push(s_pipeline_cache_hash, ph);
        sb_push(s_pipeline_cache, new_pipeline);

        bind_pipeline_from_cache(s_pipeline_cache[idx], VK_PIPELINE_BIND_POINT_COMPUTE, ph, idx);
    }

    void bind_descriptor_sets(VkCommandBuffer cmd_buf, VkPipelineBindPoint bind_point)
    {
        // cache / invalidate
        u32 nb = sb_count(_state.bindings);
        if (nb == 0)
            return;

        hash_id h = sb_hash(_state.bindings);
        if (h == _state.hdescriptors)
            return;

        // allocate a descriptor set
        VkDescriptorSet descriptor_set = 0;
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = _ctx.descriptor_pool[_ctx.ii];
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &_state.descriptor_set_layout;
        CHECK_CALL(vkAllocateDescriptorSets(_ctx.device, &alloc_info, &descriptor_set));

        _state.descriptor_set_index++;

        for (u32 i = 0; i < nb; ++i)
        {
            pen_binding& pb = _state.bindings[i];
            if (pb.index == 0)
                continue;

            VkWriteDescriptorSet descriptor_write = {};

            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = descriptor_set;
            descriptor_write.dstBinding = pb.slot;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorType = pb.descriptor_type;
            descriptor_write.descriptorCount = 1;

            VkDescriptorImageInfo image_info = {};
            VkDescriptorBufferInfo buf_info = {};

            switch (pb.descriptor_type)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                {
                    vulkan_buffer& vb = _res_pool.get(pb.index).buffer;

                    buf_info.buffer = vb.get_buffer();
                    buf_info.offset = 0;
                    buf_info.range = vb.size;

                    descriptor_write.pBufferInfo = &buf_info;
                }
                break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                {
                    vulkan_texture& vt = _res_pool.get(pb.index).texture;

                    // switch layouts for writable cs textures
                    if (bind_point == VK_PIPELINE_BIND_POINT_COMPUTE)
                    {
                        if (vt.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        {
                            _transition_image_cs(_ctx.cmd_buf_compute[_ctx.ii], vt.image,
                                vt.format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

                            vt.layout = VK_IMAGE_LAYOUT_GENERAL;
                        }
                    }

                    image_info.imageLayout = vt.layout;
                    image_info.imageView = _res_pool[pb.index].texture.image_view;
                    image_info.sampler = _res_pool[pb.sampler_index].sampler;

                    descriptor_write.pImageInfo = &image_info;
                }
                break;
            }

            vkUpdateDescriptorSets(_ctx.device, 1, &descriptor_write, 0, nullptr);
        }

        vkCmdBindDescriptorSets(cmd_buf, 
            bind_point, _state.pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

        _state.hdescriptors = h;
    }
}

namespace pen
{
    a_u64 g_gpu_total;

    static renderer_info s_renderer_info;
    const renderer_info& renderer_get_info()
    {
        s_renderer_info.caps = PEN_CAPS_TEXTURE_MULTISAMPLE
            | PEN_CAPS_DEPTH_CLAMP
            | PEN_CAPS_GPU_TIMER
            | PEN_CAPS_COMPUTE
            | PEN_CAPS_TEX_FORMAT_BC1
            | PEN_CAPS_TEX_FORMAT_BC2
            | PEN_CAPS_TEX_FORMAT_BC3
            | PEN_CAPS_TEX_FORMAT_BC4
            | PEN_CAPS_TEX_FORMAT_BC5
            ;

        return s_renderer_info;
    }

    const c8* renderer_get_shader_platform()
    {
        return "spirv";
    }

    bool renderer_viewport_vup()
    {
        return false;
    }

    namespace direct
    {
        void new_frame(u32 next_frame)
        {
            // command buffers
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

            CHECK_CALL(vkAcquireNextImageKHR(_ctx.device,
                _ctx.swap_chain, UINT64_MAX, _ctx.sem_img_avail[next_frame], nullptr, &_ctx.ii));

            vkWaitForFences(_ctx.device, 1, &_ctx.fences[_ctx.ii], VK_TRUE, (s32)-1);
            vkResetFences(_ctx.device, 1, &_ctx.fences[_ctx.ii]);

            if (_ctx.submit_flags & SUBMIT_COMPUTE)
            {
                // Use a fence to ensure that compute command buffer has finished executing before using it again
                vkWaitForFences(_ctx.device, 1, &_ctx.compute_fences[_ctx.ii], VK_TRUE, UINT64_MAX);
                vkResetFences(_ctx.device, 1, &_ctx.compute_fences[_ctx.ii]);
                _ctx.submit_flags = 0;
            }

            CHECK_CALL(vkBeginCommandBuffer(_ctx.cmd_bufs[_ctx.ii], &begin_info));

            _ctx.submit_flags |= SUBMIT_GRAPHICS;
            
            vkResetDescriptorPool(_ctx.device, _ctx.descriptor_pool[next_frame], 0);
            _state.descriptor_set_index = 0;

        }

        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res)
        {
            _res_pool.init(4096);

#if _DEBUG
            _ctx.enable_validation = true;
#endif
            enumerate_layers();
            enumerate_extensions();

            VkApplicationInfo app_info = {};
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = pen_window.window_title;
            app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            app_info.pEngineName = "pmtech";
            app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            app_info.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &app_info;
            create_info.enabledExtensionCount = sb_count(_ctx.ext_names);
            create_info.ppEnabledExtensionNames = _ctx.ext_names;

            if (_ctx.enable_validation)
            {
                static const char** debug_layer;
                sb_push(debug_layer, "VK_LAYER_KHRONOS_validation");
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = debug_layer;
            }

            CHECK_CALL(vkCreateInstance(&create_info, nullptr, &_ctx.instance));

            if(_ctx.enable_validation)
                create_debug_messenger();

            create_device_surface_swapchain(params);

            new_frame(0);

            return 0;
        }

        void renderer_shutdown()
        {
            if (_ctx.enable_validation)
                destroy_debug_messenger();

            for (u32 i = 0; i < NBB; ++i)
                vkDestroyDescriptorPool(_ctx.device, _ctx.descriptor_pool[i], nullptr);

            destroy_caches();
            destory_swapchain();

            vkDestroyCommandPool(_ctx.device, _ctx.cmd_pool, nullptr);
            vkDestroySurfaceKHR(_ctx.instance, _ctx.surface, nullptr);
            vkDestroyInstance(_ctx.instance, nullptr);
        }

        void renderer_make_context_current()
        {
            // stub.. this function is for opengl
        }

        void renderer_sync()
        {
            // stub.. this function is for metal
        }

        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            clear_state& res = _res_pool.get(resource_slot).clear;
            res = cs;
        }

        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {
            _state.clear_state = clear_state_index;

            // we may just call clear, to clear and make no draws
            begin_pass();
        }

        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);

            VkShaderModuleCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.codeSize = params.byte_code_size;
            info.pCode = (const u32*)params.byte_code;

            vulkan_shader& vs = _res_pool.get(resource_slot).shader;
            vs.type = (e_shd)params.type;
            CHECK_CALL(vkCreateShaderModule(_ctx.device, &info, nullptr, &vs.module));
        }

        void renderer_set_shader(u32 shader_index, u32 shader_type)
        {
            _state.shader[shader_type] = shader_index;
        }

        void renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            auto& res = _res_pool.get(resource_slot).vertex_attributes;
            res = nullptr;

            VkVertexInputBindingDescription* bindings = nullptr;

            for (u32 i = 0; i < params.num_elements; ++i)
            {
                VkVertexInputAttributeDescription attr;
                attr.location = i;
                attr.offset = params.input_layout[i].aligned_byte_offset;
                attr.format = to_vk_vertex_format(params.input_layout[i].format);
                attr.binding = params.input_layout[i].input_slot;

                sb_push(res, attr);
            }
        }

        void renderer_set_input_layout(u32 layout_index)
        {
            _state.input_layout = layout_index;
        }

        void renderer_link_shader_program(const shader_link_params& params, u32 resource_slot)
        {
            // stub this function is for opengl
        }

        static void _create_buffer_internal(VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props,
            void* data, u32 buffer_size, VkBuffer& buf, VkDeviceMemory& mem)
        {
            VkBufferCreateInfo info = {};
            info.size = buffer_size;
            info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            CHECK_CALL(vkCreateBuffer(_ctx.device, &info, nullptr, &buf));

            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(_ctx.device, buf, &req);

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = req.size;
            alloc_info.memoryTypeIndex = get_mem_type(req.memoryTypeBits, mem_props);

            CHECK_CALL(vkAllocateMemory(_ctx.device, &alloc_info, nullptr, &mem));
            CHECK_CALL(vkBindBufferMemory(_ctx.device, buf, mem, 0));

            if (data)
            {
                void* map_data = nullptr;
                vkMapMemory(_ctx.device, mem, 0, buffer_size, 0, &map_data);
                memcpy(map_data, data, (size_t)buffer_size);
                vkUnmapMemory(_ctx.device, mem);
            }
        }

        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            vulkan_buffer& res = _res_pool.get(resource_slot).buffer;
            res.size = params.buffer_size;

            u32 c = 1;
            res.dynamic = false;
            if (params.cpu_access_flags & PEN_CPU_ACCESS_WRITE)
            {
                res.dynamic = true;               
                c = NBB;
            }

            for (u32 i = 0; i < c; ++i)
            {
                _create_buffer_internal(to_vk_buffer_usage(params.bind_flags),
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    params.data, params.buffer_size, res.buf[i], res.mem[i]);
            }
        }

        void renderer_set_vertex_buffers(u32* buffer_indices, 
            u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets)
        {
            if (_state.vertex_input_bindings)
            {
                sb_free(_state.vertex_input_bindings);
                _state.vertex_input_bindings = nullptr;
            }

            static VkBuffer _bufs[8];
            static VkDeviceSize _offsets[8];
            for (u32 i = 0; i < num_buffers; ++i)
            {
                _bufs[i] = _res_pool.get(buffer_indices[i]).buffer.get_buffer();
                _offsets[i] = offsets[i];

                VkVertexInputBindingDescription vb;
                vb.binding = i;
                vb.inputRate = i == 0 ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
                vb.stride = strides[i];

                sb_push(_state.vertex_input_bindings, vb);
            }

            vkCmdBindVertexBuffers(_ctx.cmd_bufs[_ctx.ii], start_slot, num_buffers, _bufs, _offsets);
        }

        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
        {
            VkBuffer buf = _res_pool.get(buffer_index).buffer.get_buffer();
            vkCmdBindIndexBuffer(_ctx.cmd_bufs[_ctx.ii], buf, offset, to_vk_index_type(format));
        }

        inline void _set_binding(const pen_binding& b)
        {
            u32 num = sb_count(_state.bindings);
            for (u32 i = 0; i < num; ++i)
            {
                if (_state.bindings[i].slot == b.slot)
                {
                    _state.bindings[i] = b;
                    return;
                }
            }

            sb_push(_state.bindings, b);
        }

        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            if (buffer_index == 0)
                return;

            pen_binding b;
            b.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b.stage = to_vk_stage(flags);
            b.index = buffer_index;
            b.slot = resource_slot;
            b.bind_flags = flags;

            _set_binding(b);
        }
        
        void renderer_set_structured_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
        {
            
        }

        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
        {
            if (data_size == 0)
                return;

            VkDeviceMemory mem = _res_pool.get(buffer_index).buffer.get_mem();

            void* map_data;
            vkMapMemory(_ctx.device, mem, 0, data_size, 0, &map_data);
            memcpy(map_data, data, (size_t)data_size);
            vkUnmapMemory(_ctx.device, mem);
        }

        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            vulkan_texture& vt = _res_pool.get(resource_slot).texture;

            // take a cpy of tcp, we might need it later
            vt.tcp = new texture_creation_params(renderer_tcp_resolve_ratio(tcp));
            vt.format = to_vk_image_format(vt.tcp->format);

            if (vt.tcp->bind_flags & PEN_BIND_SHADER_WRITE)
                vt.compute_shader_write = true;

            // image
            VkImageCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.extent.width = vt.tcp->width;
            info.extent.height = vt.tcp->height;
            info.extent.depth = vt.tcp->collection_type == TEXTURE_COLLECTION_VOLUME ? vt.tcp->num_arrays : 1;
            info.arrayLayers = 1;
            info.samples = (VkSampleCountFlagBits)vt.tcp->sample_count;
            info.mipLevels = vt.tcp->num_mips;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = vt.format;
            info.usage = to_vk_texture_usage(vt.tcp->bind_flags, vt.tcp->format, vt.tcp->data);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            CHECK_CALL(vkCreateImage(_ctx.device, &info, nullptr, &vt.image));

            // memory
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(_ctx.device, vt.image, &req);

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = req.size;
            alloc_info.memoryTypeIndex = get_mem_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            CHECK_CALL(vkAllocateMemory(_ctx.device, &alloc_info, nullptr, &vt.mem));
            CHECK_CALL(vkBindImageMemory(_ctx.device, vt.image, vt.mem, 0));

            // image view
            VkImageViewCreateInfo view_info = {};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = vt.image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = vt.format;
            view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.subresourceRange.aspectMask = to_vk_image_aspect(vt.tcp->format);
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            CHECK_CALL(vkCreateImageView(_ctx.device, &view_info, nullptr, &vt.image_view));

            if (vt.tcp->data)
            {
                VkBuffer buf;
                VkDeviceMemory mem;
                _create_buffer_internal(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    vt.tcp->data, vt.tcp->data_size, buf, mem);

                _transition_image(vt.image, 
                    vt.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkBufferImageCopy region = {};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = { 0, 0, 0 };
                region.imageExtent = {
                    vt.tcp->width,
                    vt.tcp->height,
                    1
                };

                VkCommandBuffer cmd = begin_cmd_buffer();
                vkCmdCopyBufferToImage(cmd, buf, vt.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                end_cmd_buffer(cmd);

                _transition_image(vt.image,
                    vt.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                vkDestroyBuffer(_ctx.device, buf, nullptr);
                vkFreeMemory(_ctx.device, mem, nullptr);
            }
            else
            {
                _transition_image(vt.image,
                    vt.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            vt.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        void renderer_create_sampler(const sampler_creation_params& scp, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            VkSampler& vs = _res_pool.get(resource_slot).sampler;

            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

            info.magFilter = to_vk_filter(scp.filter);
            info.minFilter = to_vk_filter(scp.filter);
            info.mipmapMode = to_vk_mip_map_mode(scp.filter);

            info.addressModeU = to_vk_sampler_address_mode(scp.address_u);
            info.addressModeV = to_vk_sampler_address_mode(scp.address_v);
            info.addressModeW = to_vk_sampler_address_mode(scp.address_w);

            info.anisotropyEnable = scp.max_anisotropy > 0;
            info.maxAnisotropy = scp.max_anisotropy;

            info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

            info.unnormalizedCoordinates = VK_FALSE;

            info.compareEnable = VK_FALSE;
            info.compareOp = VK_COMPARE_OP_ALWAYS;

            info.mipLodBias = scp.mip_lod_bias;
            info.minLod = scp.min_lod;
            info.maxLod = scp.max_lod;

            CHECK_CALL(vkCreateSampler(_ctx.device, &info, nullptr, &vs));
        }

        void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags)
        {
            if (texture_index == 0)
                return;

            vulkan_texture& vt = _res_pool.get(texture_index).texture;

            pen_binding b;
            b.descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.stage = to_vk_stage(bind_flags);
            if (bind_flags & pen::TEXTURE_BIND_CS)
                b.descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            b.index = texture_index;
            b.sampler_index = sampler_index;
            b.slot = resource_slot;
            b.bind_flags = bind_flags;

            _set_binding(b);
        }

        void renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            VkPipelineRasterizationStateCreateInfo& res = _res_pool.get(resource_slot).raster;

            res = {};
            res.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            res.depthClampEnable = VK_FALSE;
            res.depthBiasEnable = VK_FALSE;
            res.lineWidth = 1.0f;
            res.polygonMode = to_vk_polygon_mode(rscp.fill_mode);
            res.cullMode = to_vk_cull_mode(rscp.cull_mode);
            res.frontFace = rscp.front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        }

        void renderer_set_rasterizer_state(u32 rasterizer_state_index)
        {
            _state.raster = rasterizer_state_index;
        }

        void renderer_set_viewport(const viewport& vp)
        {
            _state.vp = vp;
        }

        void renderer_set_scissor_rect(const rect& r)
        {
            _state.sr = r;
        }

        void renderer_create_blend_state(const blend_creation_params& bcp, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            vulkan_blend_state& bs = _res_pool.get(resource_slot).blend;

            bs.attachments = nullptr;
            for (u32 i = 0; i < bcp.num_render_targets; ++i)
            {
                pen::render_target_blend& rtb = bcp.render_targets[i];

                VkPipelineColorBlendAttachmentState attach = {};
                attach.colorWriteMask = rtb.render_target_write_mask;
                attach.blendEnable = rtb.blend_enable;

                if (rtb.blend_enable)
                {
                    attach.colorBlendOp = to_vk_blend_op(rtb.blend_op);
                    attach.srcColorBlendFactor = to_vk_blend_factor(rtb.src_blend);
                    attach.dstColorBlendFactor = to_vk_blend_factor(rtb.dest_blend);

                    if (bcp.independent_blend_enable)
                    {
                        attach.alphaBlendOp = to_vk_blend_op(rtb.blend_op_alpha);
                        attach.srcAlphaBlendFactor = to_vk_blend_factor(rtb.src_blend_alpha);
                        attach.dstAlphaBlendFactor = to_vk_blend_factor(rtb.dest_blend_alpha);
                    }
                    else
                    {
                        attach.alphaBlendOp = attach.colorBlendOp;
                        attach.srcAlphaBlendFactor = attach.srcColorBlendFactor;
                        attach.dstAlphaBlendFactor = attach.dstColorBlendFactor;
                    }
                }

                sb_push(bs.attachments, attach);
            }

            bs.info = {};
            bs.info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            bs.info.logicOpEnable = VK_FALSE;
            bs.info.logicOp = VK_LOGIC_OP_COPY;
            bs.info.attachmentCount = sb_count(bs.attachments);
            bs.info.pAttachments = bs.attachments;
            bs.info.blendConstants[0] = 0.0f;
            bs.info.blendConstants[1] = 0.0f;
            bs.info.blendConstants[2] = 0.0f;
            bs.info.blendConstants[3] = 0.0f;
        }

        void renderer_set_blend_state(u32 blend_state_index)
        {
            _state.blend = blend_state_index;
        }

        void renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot)
        {

        }

        void renderer_set_depth_stencil_state(u32 depth_stencil_state)
        {

        }

        void renderer_set_stencil_ref(u8 ref)
        {

        }

        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
        {
            begin_pass();
            bind_render_pipeline(primitive_topology);
            bind_descriptor_sets(_ctx.cmd_bufs[_ctx.ii], VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdDraw(_ctx.cmd_bufs[_ctx.ii], vertex_count, 1, 0, 0);
        }

        inline void _draw_index_instanced(u32 instance_count, 
            u32 start_instance, u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
        {
            begin_pass();
            bind_render_pipeline(primitive_topology);
            bind_descriptor_sets(_ctx.cmd_bufs[_ctx.ii], VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdDrawIndexed(_ctx.cmd_bufs[_ctx.ii], 
                index_count, instance_count, start_index, base_vertex, start_instance);
        }

        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
        {
            _draw_index_instanced(1, 0, index_count, start_index, base_vertex, primitive_topology);
        }

        void renderer_draw_indexed_instanced(u32 instance_count, 
            u32 start_instance, u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
        {
            _draw_index_instanced(instance_count, start_instance, index_count, start_index, base_vertex, primitive_topology);
        }

        void renderer_draw_auto()
        {

        }

        void renderer_dispatch_compute(uint3 grid, uint3 num_threads)
        {
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            vkBeginCommandBuffer(_ctx.cmd_buf_compute[_ctx.ii], &begin_info);

            bind_compute_pipeline();
            bind_descriptor_sets(_ctx.cmd_buf_compute[_ctx.ii], VK_PIPELINE_BIND_POINT_COMPUTE);

            vkCmdDispatch(_ctx.cmd_buf_compute[_ctx.ii], grid.x / num_threads.x, grid.y / num_threads.y, grid.z / num_threads.z);

            vkEndCommandBuffer(_ctx.cmd_buf_compute[_ctx.ii]);

            _ctx.submit_flags |= SUBMIT_COMPUTE;

            sb_free(_state.bindings);
            _state.bindings = nullptr;
        }

        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
            return renderer_create_texture(tcp, resource_slot);
        }

        void renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face, 
            u32 depth_face)
        {
            sb_free(_state.bindings);
            _state.bindings = nullptr;

            sb_clear(_state.colour_attachments);
            _state.colour_attachments = nullptr;

            for (u32 i = 0; i < num_colour_targets; ++i)
            {
                u32 ct = colour_targets[i];
                if (ct == 0)
                    ct += _ctx.ii;
                sb_push(_state.colour_attachments, ct);
            }

            _state.depth_attachment = depth_target;
            _state.depth_slice = depth_face;
            _state.colour_slice = colour_face;
        }

        void renderer_set_resolve_targets(u32 colour_target, u32 depth_target)
        {

        }

        void renderer_set_stream_out_target(u32 buffer_index)
        {

        }

        void renderer_resolve_target(u32 target, e_msaa_resolve_type type, resolve_resources res)
        {

        }

        void renderer_read_back_resource(const resource_read_back_params& rrbp)
        {

        }

        void renderer_present()
        {
            end_render_pass();

            vkEndCommandBuffer(_ctx.cmd_bufs[_ctx.ii]);

            if (_ctx.submit_flags & SUBMIT_COMPUTE)
            {
                // submit compute
                VkSubmitInfo compute_submit_info = {};
                compute_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                compute_submit_info.commandBufferCount = 1;
                compute_submit_info.pCommandBuffers = &_ctx.cmd_buf_compute[_ctx.ii];

                CHECK_CALL(vkQueueSubmit(_ctx.compute_queue, 1, &compute_submit_info, _ctx.compute_fences[_ctx.ii]));
            }

            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &_ctx.cmd_bufs[_ctx.ii];
            
            // wait
            VkSemaphore sem_wait[] = { _ctx.sem_img_avail[_ctx.ii] };
            VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = sem_wait;
            info.pWaitDstStageMask = wait_stages;

            // signal
            VkSemaphore sem_signal[] = { _ctx.sem_render_finished[_ctx.ii] };
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = sem_signal;
            info.signalSemaphoreCount = 1;

            CHECK_CALL(vkQueueSubmit(_ctx.graphics_queue, 1, &info, _ctx.fences[_ctx.ii]));

            VkSwapchainKHR swapChains[] = { _ctx.swap_chain };
            VkPresentInfoKHR present = {};
            present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.swapchainCount = 1;
            present.pSwapchains = swapChains;
            present.pImageIndices = &_ctx.ii;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = sem_signal;

            // present and swap
            VkResult result = vkQueuePresentKHR(_ctx.present_queue, &present);
            u32 next_frame = (_ctx.ii + 1) % NBB;

            // handle swapchain re-creation / window resize
            if (result == VK_ERROR_OUT_OF_DATE_KHR ||
                result == VK_SUBOPTIMAL_KHR ||
                g_window_resize)
            {
                g_window_resize = 0;
                vkDeviceWaitIdle(_ctx.device);
                create_swapchain();
                next_frame = 0;
            }

            new_frame(next_frame);
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
            vulkan_shader& vs = _res_pool.get(shader_index).shader;
            vkDestroyShaderModule(_ctx.device, vs.module, nullptr);
        }

        void renderer_release_clear_state(u32 clear_state)
        {
            // no mem to free
        }

        void renderer_release_buffer(u32 buffer_index)
        {
            vulkan_buffer& buf = _res_pool.get(buffer_index).buffer;
            u32 c = buf.dynamic ? NBB : 1;
            
            for (u32 i = 0; i < c; ++i)
            {
                vkDestroyBuffer(_ctx.device, buf.buf[i], nullptr);
                vkFreeMemory(_ctx.device, buf.mem[i], nullptr);
            }
        }

        void renderer_release_texture(u32 texture_index)
        {
            vulkan_texture& vt = _res_pool.get(texture_index).texture;

            vkDestroyImage(_ctx.device, vt.image, nullptr);
            vkDestroyImageView(_ctx.device, vt.image_view, nullptr);
            vkFreeMemory(_ctx.device, vt.mem, nullptr);
        }

        void renderer_release_sampler(u32 sampler)
        {
            vkDestroySampler(_ctx.device, _res_pool.get(sampler).sampler, nullptr);
        }

        void renderer_release_raster_state(u32 raster_state_index)
        {

        }

        void renderer_release_blend_state(u32 blend_state)
        {
            vulkan_blend_state& res = _res_pool.get(blend_state).blend;
            sb_free(res.attachments);
        }

        void renderer_release_render_target(u32 render_target)
        {
            renderer_release_texture(render_target);
        }

        void renderer_release_input_layout(u32 input_layout)
        {
            VkVertexInputAttributeDescription* vi = _res_pool.get(input_layout).vertex_attributes;
            sb_free(vi);
        }

        void renderer_release_depth_stencil_state(u32 depth_stencil_state)
        {

        }
    }
}

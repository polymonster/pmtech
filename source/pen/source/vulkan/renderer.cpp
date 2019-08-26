#include "renderer.h"
#include "console.h"
#include "data_struct.h"

#include "vulkan/vulkan.h"

#ifdef _WIN32
#include "vulkan/vulkan_win32.h"
#endif

#define CHECK_CALL(C) { VkResult r = (C); PEN_ASSERT(r == VK_SUCCESS); }

extern pen::window_creation_params pen_window;
a_u8                               g_window_resize(0);

using namespace pen;

#define NBB 3

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
        return 0;
    }

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
        VkDevice                            device;
        VkSurfaceKHR                        surface;
        VkSwapchainKHR                      swap_chain;
        VkImage*                            swap_chain_images = nullptr;
        VkCommandPool                       cmd_pool;
        VkCommandBuffer*                    cmd_bufs = nullptr;
        u32                                 img_index = 0;
        VkSemaphore                         sem_img_avail[NBB];
        VkSemaphore                         sem_render_finished[NBB];
        VkFence                             fences[NBB];
        VkPhysicalDeviceMemoryProperties    mem_properties;
    };
    vulkan_context _ctx;

    struct pen_state
    {
        u32             shader[3]; // vs, fs, cs
        u32*            colour_attachments = nullptr;
        u32             depth_attachment = 0;
        u32             colour_slice;
        u32             depth_slice;
        u32             clear_state;
        viewport        vp;
        rect            sr;
        u32             vertex_buffer;
        u32             index_buffer;
        u32             input_layout;
        u32             raster;
        VkRenderPass    pass;
        VkVertexInputBindingDescription* vertex_input_bindings = nullptr;
    };
    pen_state _state;

    struct vulkan_texture
    {
        VkImageView image_view;
        VkImage     image;
        VkFormat    format;
    };

    struct vulkan_buffer
    {
        VkBuffer        buf;
        VkDeviceMemory  mem;
    };

    enum e_shd
    {
        vertex,
        fragment,
        compute
    };

    struct vulkan_shader
    {
        VkShaderModule module;
        e_shd          type;
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
        };
    };
    res_pool<resource_allocation> _res_pool;

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
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
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
        buf_info.commandBufferCount = sb_count(_ctx.swap_chain_images);
        buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        // make enough size
        for (u32 i = 0; i < buf_info.commandBufferCount; ++i)
            sb_push(_ctx.cmd_bufs, VkCommandBuffer());

        CHECK_CALL(vkAllocateCommandBuffers(_ctx.device, &buf_info, _ctx.cmd_bufs));
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
        }
    }

    void create_device_surface_swapchain(void* params)
    {
        u32 dev_count;
        CHECK_CALL(vkEnumeratePhysicalDevices(_ctx.instance, &dev_count, nullptr));
        PEN_ASSERT(dev_count); // no supported devices

        VkPhysicalDevice* devices = new VkPhysicalDevice[dev_count];
        CHECK_CALL(vkEnumeratePhysicalDevices(_ctx.instance, &dev_count, devices));

        _ctx.physical_device = devices[0];

        // find gfx queue
        _ctx.num_queue_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(_ctx.physical_device, &_ctx.num_queue_families, nullptr);

        VkQueueFamilyProperties* queue_families = new VkQueueFamilyProperties[_ctx.num_queue_families];
        vkGetPhysicalDeviceQueueFamilyProperties(_ctx.physical_device, &_ctx.num_queue_families, queue_families);

        // surface
        create_surface(params);
        
        _ctx.graphics_family_index = -1;
        _ctx.present_family_index = -1;
        for (u32 i = 0; i < _ctx.num_queue_families; ++i)
        {
            if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                _ctx.graphics_family_index = i;
            }

            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(_ctx.physical_device, i, _ctx.surface, &present);
            if (queue_families[i].queueCount > 0 && present)
            {
                _ctx.present_family_index = i;
            }
        }
        PEN_ASSERT(_ctx.graphics_family_index != -1);
        PEN_ASSERT(_ctx.present_family_index != -1);

        // gfx queue
        f32 pri = 1.0f;
        VkDeviceQueueCreateInfo gfx_queue_info = {};
        gfx_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gfx_queue_info.queueFamilyIndex = _ctx.graphics_family_index;
        gfx_queue_info.queueCount = 1;
        gfx_queue_info.pQueuePriorities = &pri;

        VkDeviceQueueCreateInfo present_queue_info = {};
        present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_info.queueFamilyIndex = _ctx.present_family_index;
        present_queue_info.queueCount = 1;
        present_queue_info.pQueuePriorities = &pri;

        VkDeviceQueueCreateInfo* queues = nullptr;
        sb_push(queues, gfx_queue_info);
        sb_push(queues, present_queue_info);

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

        // swap chain

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


        VkSurfaceCapabilitiesKHR caps;
        CHECK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_ctx.physical_device, _ctx.surface, &caps));

        VkSwapchainCreateInfoKHR swap_chain_info = {};
        swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swap_chain_info.surface = _ctx.surface;
        swap_chain_info.minImageCount = 3;
        swap_chain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swap_chain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swap_chain_info.imageExtent = { pen_window.width, pen_window.height };
        swap_chain_info.imageArrayLayers = 1;
        swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swap_chain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swap_chain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swap_chain_info.oldSwapchain = VK_NULL_HANDLE;
        swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swap_chain_info.clipped = VK_TRUE;
        swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        CHECK_CALL(vkCreateSwapchainKHR(_ctx.device, &swap_chain_info, nullptr, &_ctx.swap_chain));

        create_backbuffer_targets();

        create_command_buffers();

        create_sync_primitives();

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

    void begin_pass()
    {
        const clear_state& clear = _res_pool.get(_state.clear_state).clear;

        // attachments
        VkAttachmentDescription* colour_attachments = nullptr;
        VkAttachmentReference*   colour_refs = nullptr;
        VkImageView*             colour_img_view = nullptr;

        for (u32 i = 0; i < sb_count(_state.colour_attachments); ++i)
        {
            u32 ica = _state.colour_attachments[i];
            const vulkan_texture& vt = _res_pool.get(ica + _ctx.img_index).texture;

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
            col.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
        fb_info.width = pen_window.width;
        fb_info.height = pen_window.height;
        fb_info.layers = 1;

        VkFramebuffer fb;
        CHECK_CALL(vkCreateFramebuffer(_ctx.device, &fb_info, nullptr, &fb));

        sb_free(colour_attachments);
        sb_free(colour_refs);
        sb_free(colour_img_view);

        // clear
        clear_state& cs = _res_pool.get(_state.clear_state).clear;
        VkClearColorValue clear_colour = { cs.r, cs.g, cs.b, cs.a };
        VkClearValue clear_value = {};
        clear_value.color = clear_colour;

        VkImageSubresourceRange img_range = {};
        img_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_range.levelCount = 1;
        img_range.layerCount = 1;

        VkRenderPassBeginInfo rp_begin_info = {};
        rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin_info.renderPass = pass;
        rp_begin_info.framebuffer = fb;
        rp_begin_info.renderArea.offset = { 0, 0 };
        rp_begin_info.renderArea.extent = { 1280, 720 };
        rp_begin_info.clearValueCount = 1;
        rp_begin_info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(_ctx.cmd_bufs[_ctx.img_index], &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        _state.pass = pass;
    }

    void bind_pipeline(u32 primitive_topology)
    {
        // check for invalidation

        // create pipeline

        // raster
        VkGraphicsPipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pRasterizationState = &_res_pool.get(_state.raster).raster;

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = to_vk_primitive_topology(primitive_topology);
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        info.pInputAssemblyState = &inputAssembly;

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
        scissor.extent = { (u32)sr.right - (u32)sr.left, (u32)sr.top - (u32)sr.bottom };

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
        vertex_shader_info.pName = "vs_main";

        VkPipelineShaderStageCreateInfo fragment_shader_info = {};
        fragment_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_shader_info.module = _res_pool.get(fs).shader.module;
        fragment_shader_info.pName = "ps_main";

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

        // multisample

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        info.pMultisampleState = &multisampling;

        // layout
        VkPipelineLayout pipeline_layout;
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pushConstantRangeCount = 0;

        CHECK_CALL(vkCreatePipelineLayout(_ctx.device, &pipeline_layout_info, nullptr, &pipeline_layout));

        info.layout = pipeline_layout;

        // pass

        info.renderPass = _state.pass;
        info.subpass = 0;
        info.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline pipeline;
        CHECK_CALL(vkCreateGraphicsPipelines(_ctx.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

        vkCmdBindPipeline(_ctx.cmd_bufs[_ctx.img_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }
}

namespace pen
{
    a_u64 g_gpu_total;

    static renderer_info s_renderer_info;
    const renderer_info& renderer_get_info()
    {
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
                _ctx.swap_chain, UINT64_MAX, _ctx.sem_img_avail[next_frame], nullptr, &_ctx.img_index));

            vkWaitForFences(_ctx.device, 1, &_ctx.fences[_ctx.img_index], VK_TRUE, (s32)-1);
            vkResetFences(_ctx.device, 1, &_ctx.fences[_ctx.img_index]);

            CHECK_CALL(vkBeginCommandBuffer(_ctx.cmd_bufs[_ctx.img_index], &begin_info));
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

            // to query memory type
            vkGetPhysicalDeviceMemoryProperties(_ctx.physical_device, &_ctx.mem_properties);

            new_frame(0);

            return 0;
        }

        void renderer_shutdown()
        {
            if (_ctx.enable_validation)
                destroy_debug_messenger();

            for (u32 i = 0; i < sb_count(_ctx.swap_chain_images); ++i)
                vkDestroyImage(_ctx.device, _ctx.swap_chain_images[i], nullptr);

            vkDestroySurfaceKHR(_ctx.instance, _ctx.surface, nullptr);
            vkDestroySwapchainKHR(_ctx.device, _ctx.swap_chain, nullptr);
            vkDestroyInstance(_ctx.instance, nullptr);
        }

        void renderer_make_context_current()
        {

        }

        void renderer_sync()
        {

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

        }

        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot)
        {
            _res_pool.insert({}, resource_slot);
            vulkan_buffer& res = _res_pool.get(resource_slot).buffer;

            VkBufferCreateInfo info = {};
            info.size = params.buffer_size;
            info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            info.usage = to_vk_buffer_usage(params.bind_flags);
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            CHECK_CALL(vkCreateBuffer(_ctx.device, &info, nullptr, &res.buf));

            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(_ctx.device, res.buf, &req);

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = req.size;
            alloc_info.memoryTypeIndex = get_mem_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            CHECK_CALL(vkAllocateMemory(_ctx.device, &alloc_info, nullptr, &res.mem));
            CHECK_CALL(vkBindBufferMemory(_ctx.device, res.buf, res.mem, 0));

            if (params.data)
            {
                void* data = nullptr;
                vkMapMemory(_ctx.device, res.mem, 0, params.buffer_size, 0, &data);
                memcpy(data, params.data, (size_t)params.buffer_size);
                vkUnmapMemory(_ctx.device, res.mem);
            }
        }

        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets)
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
                _bufs[i] = _res_pool.get(buffer_indices[i]).buffer.buf;
                _offsets[i] = offsets[i];

                VkVertexInputBindingDescription vb;
                vb.binding = i;
                vb.inputRate = i == 0 ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
                vb.stride = strides[i];

                sb_push(_state.vertex_input_bindings, vb);
            }

            vkCmdBindVertexBuffers(_ctx.cmd_bufs[_ctx.img_index], start_slot, num_buffers, _bufs, _offsets);
        }

        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
        {

        }

        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
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
            _res_pool.insert({}, resource_slot);
            VkPipelineRasterizationStateCreateInfo& res = _res_pool.get(resource_slot).raster;

            res = {};
            res.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            res.depthClampEnable = VK_FALSE;
            res.depthBiasEnable = VK_FALSE;
            res.lineWidth = 1.0f;
            res.rasterizerDiscardEnable = rscp.depth_clip_enable;
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

        void renderer_set_stencil_ref(u8 ref)
        {

        }

        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
        {
            bind_pipeline(primitive_topology);

            vkCmdDraw(_ctx.cmd_bufs[_ctx.img_index], vertex_count, 1, 0, 0);
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

        void renderer_dispatch_compute(uint3 grid, uint3 num_threads)
        {

        }

        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {

        }

        void renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face)
        {
            sb_clear(_state.colour_attachments);
            _state.colour_attachments = nullptr;

            for (u32 i = 0; i < num_colour_targets; ++i)
                sb_push(_state.colour_attachments, colour_targets[i]);

            _state.depth_attachment = depth_target;
            _state.depth_slice = depth_face;
            _state.colour_slice = colour_face;

            begin_pass();
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
            vkCmdEndRenderPass(_ctx.cmd_bufs[_ctx.img_index]);
            vkEndCommandBuffer(_ctx.cmd_bufs[_ctx.img_index]);

            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &_ctx.cmd_bufs[_ctx.img_index];
            
            // wait
            VkSemaphore sem_wait[] = { _ctx.sem_img_avail[_ctx.img_index] };
            VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = sem_wait;
            info.pWaitDstStageMask = wait_stages;

            // signal
            VkSemaphore sem_signal[] = { _ctx.sem_render_finished[_ctx.img_index] };
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = sem_signal;
            info.signalSemaphoreCount = 1;

            CHECK_CALL(vkQueueSubmit(_ctx.graphics_queue, 1, &info, _ctx.fences[_ctx.img_index]));

            VkSwapchainKHR swapChains[] = { _ctx.swap_chain };
            VkPresentInfoKHR present = {};
            present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.swapchainCount = 1;
            present.pSwapchains = swapChains;
            present.pImageIndices = &_ctx.img_index;
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = sem_signal;

            CHECK_CALL(vkQueuePresentKHR(_ctx.present_queue, &present));

            u32 next_frame = (_ctx.img_index + 1) % NBB;
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

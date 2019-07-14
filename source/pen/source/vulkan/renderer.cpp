#include "renderer.h"
#include "console.h"
#include "data_struct.h"

#include "vulkan/vulkan.h"

#ifdef _WIN32
#include "vulkan/vulkan_win32.h"
#endif

extern pen::window_creation_params pen_window;
a_u8                               g_window_resize(0);

namespace
{
    struct vulkan_context
    {
        VkInstance                  instance;
        u32                         num_layer_props;
        VkLayerProperties*          layer_props;
        const char**                layer_names = nullptr;
        u32                         num_ext_props;
        VkExtensionProperties*      ext_props;
        const char**                ext_names = nullptr;
        VkDebugUtilsMessengerEXT    debug_messanger;
        bool                        enable_validation = false;
        VkPhysicalDevice            physical_device = VK_NULL_HANDLE;
        u32                         num_queue_families;
        VkQueue                     graphics_queue;
        VkQueue                     present_queue;
        VkDevice                    device;
        VkSurfaceKHR                surface;
        VkSwapchainKHR              swap_chain;
    };
    vulkan_context _context;

    void enumerate_layers()
    {
        vkEnumerateInstanceLayerProperties(&_context.num_layer_props, nullptr);

        _context.layer_props = new VkLayerProperties[_context.num_layer_props];
        vkEnumerateInstanceLayerProperties(&_context.num_layer_props, _context.layer_props);

        for (u32 i = 0; i < _context.num_layer_props; ++i)
            sb_push(_context.layer_names, _context.layer_props[i].layerName);
    }

    void enumerate_extensions()
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &_context.num_ext_props, nullptr);

        _context.ext_props = new VkExtensionProperties[_context.num_ext_props];
        vkEnumerateInstanceExtensionProperties(nullptr, &_context.num_ext_props, _context.ext_props);

        for (u32 i = 0; i < _context.num_ext_props; ++i)
            sb_push(_context.ext_names, _context.ext_props[i].extensionName);
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

        VkResult res = CreateDebugUtilsMessengerEXT(_context.instance, &info, nullptr, &_context.debug_messanger);
        PEN_ASSERT(res == VK_SUCCESS);
    }

    void destroy_debug_messenger()
    {
        DestroyDebugUtilsMessengerEXT(_context.instance, _context.debug_messanger, nullptr);
    }

    void create_surface(void* params)
    {
        // in win32 params is HWND
        HWND hwnd = *((HWND*)params);

        VkWin32SurfaceCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hwnd = hwnd;
        info.hinstance = GetModuleHandle(nullptr);

        VkResult res = vkCreateWin32SurfaceKHR(_context.instance, &info, nullptr, &_context.surface);
        PEN_ASSERT(res == VK_SUCCESS);
    }

    void create_device_surface_swapchain(void* params)
    {
        u32 dev_count;
        vkEnumeratePhysicalDevices(_context.instance, &dev_count, nullptr);
        PEN_ASSERT(dev_count); // no supported devices

        VkPhysicalDevice* devices = new VkPhysicalDevice[dev_count];
        vkEnumeratePhysicalDevices(_context.instance, &dev_count, devices);

        _context.physical_device = devices[0];

        // find gfx queue
        _context.num_queue_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(_context.physical_device, &_context.num_queue_families, nullptr);

        VkQueueFamilyProperties* queue_families = new VkQueueFamilyProperties[_context.num_queue_families];
        vkGetPhysicalDeviceQueueFamilyProperties(_context.physical_device, &_context.num_queue_families, queue_families);

        // surface
        create_surface(params);
        
        u32 graphics_family_index = -1;
        u32 present_family_index = -1;
        for (u32 i = 0; i < _context.num_queue_families; ++i)
        {
            if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphics_family_index = i;
            }

            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(_context.physical_device, i, _context.surface, &present);
            if (queue_families[i].queueCount > 0 && present)
            {
                present_family_index = i;
            }
        }
        PEN_ASSERT(graphics_family_index != -1);
        PEN_ASSERT(present_family_index != -1);

        // gfx queue
        f32 pri = 1.0f;
        VkDeviceQueueCreateInfo gfx_queue_info = {};
        gfx_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gfx_queue_info.queueFamilyIndex = graphics_family_index;
        gfx_queue_info.queueCount = 1;
        gfx_queue_info.pQueuePriorities = &pri;

        VkDeviceQueueCreateInfo present_queue_info = {};
        present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_info.queueFamilyIndex = present_family_index;
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
        if (_context.enable_validation)
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

        VkResult res = vkCreateDevice(_context.physical_device, &info, nullptr, &_context.device);
        PEN_ASSERT(res == VK_SUCCESS);

        vkGetDeviceQueue(_context.device, graphics_family_index, 0, &_context.graphics_queue);
        vkGetDeviceQueue(_context.device, present_family_index, 0, &_context.present_queue);

        // swap chain
        u32 num_formats;
        vkGetPhysicalDeviceSurfaceFormatsKHR(_context.physical_device, _context.surface, &num_formats, nullptr);
        PEN_ASSERT(num_formats);

        VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[num_formats];
        vkGetPhysicalDeviceSurfaceFormatsKHR(_context.physical_device, _context.surface, &num_formats, formats);

        u32 num_present_modes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(_context.physical_device, _context.surface, &num_present_modes, nullptr);
        PEN_ASSERT(num_present_modes);

        VkPresentModeKHR* present_modes = new VkPresentModeKHR[num_present_modes];
        vkGetPhysicalDeviceSurfacePresentModesKHR(_context.physical_device, _context.surface, &num_formats, present_modes);

        VkSurfaceCapabilitiesKHR caps;
        res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_context.physical_device, _context.surface, &caps);
        PEN_ASSERT(res == VK_SUCCESS);

        VkSwapchainCreateInfoKHR swap_chain_info = {};
        swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swap_chain_info.surface = _context.surface;
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

        res = vkCreateSwapchainKHR(_context.device, &swap_chain_info, nullptr, &_context.swap_chain);
        PEN_ASSERT(res == VK_SUCCESS);

        delete devices;
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
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res)
        {
#if _DEBUG
            _context.enable_validation = true;
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
            create_info.enabledExtensionCount = _context.num_ext_props;
            create_info.ppEnabledExtensionNames = _context.ext_names;

            if (_context.enable_validation)
            {
                static const char** debug_layer;
                sb_push(debug_layer, "VK_LAYER_KHRONOS_validation");
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = debug_layer;
            }

            VkResult result = vkCreateInstance(&create_info, nullptr, &_context.instance);
            PEN_ASSERT(result == VK_SUCCESS);

            if(_context.enable_validation)
                create_debug_messenger();

            create_device_surface_swapchain(params);

            return 0;
        }

        void renderer_shutdown()
        {
            if (_context.enable_validation)
                destroy_debug_messenger();

            vkDestroySurfaceKHR(_context.instance, _context.surface, nullptr);
            vkDestroySwapchainKHR(_context.device, _context.swap_chain, nullptr);
            vkDestroyInstance(_context.instance, nullptr);
        }

        void renderer_make_context_current()
        {

        }

        void renderer_sync()
        {

        }

        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot)
        {

        }

        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {

        }

        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot)
        {

        }

        void renderer_set_shader(u32 shader_index, u32 shader_type)
        {

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

        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot)
        {

        }

        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets)
        {

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

        }

        void renderer_set_rasterizer_state(u32 rasterizer_state_index)
        {

        }

        void renderer_set_viewport(const viewport& vp)
        {

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

        void renderer_set_stencil_ref(u8 ref)
        {

        }

        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
        {

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

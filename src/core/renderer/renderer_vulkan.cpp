#ifdef RENDERER_VULKAN
#include "railguard/core/renderer.h"
#include <railguard/core/window.h>
#include <railguard/utils/array.h>

#include <iostream>
#include <string>
#include <volk.h>

// ---==== Defines ====---

#define VULKAN_API_VERSION VK_API_VERSION_1_1

namespace rg
{
    // ---==== Types ====---

    struct Queue
    {
        uint32_t family_index = 0;
        VkQueue  queue        = VK_NULL_HANDLE;
    };

    struct Renderer::Data
    {
        VkInstance       instance        = VK_NULL_HANDLE;
        VkDevice         device          = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        Queue            graphics_queue  = {};
#ifdef USE_VK_VALIDATION_LAYERS
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#endif
    };

    // ---==== Utility functions ====---

    // region Checks

    std::string vk_result_to_string(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            default: return std::to_string(result);
        }
    }

    void vk_check(VkResult result, const std::string &error_message = "")
    {
        if (result != VK_SUCCESS)
        {
            // Pretty print error
            std::cerr << "[Vulkan Error] A Vulkan function call returned VkResult = " << vk_result_to_string(result) << "\n";

            // Optional custom error message precision
            if (!error_message.empty())
            {
                std::cerr << "Precision: " << error_message << "\n";
            }
        }
    }

    void check(bool result, const std::string &error_message)
    {
        if (!result)
        {
            std::cerr << "[Error] " << error_message << " Aborting.\n";
            // Completely halt the program
            // TODO maybe recoverable ?
            exit(1);
        }
    }

    // endregion

    // region Extensions and layers functions

    bool check_instance_extension_support(const Array<const char *> &desired_extensions)
    {
        // Get the number of available desired_extensions
        uint32_t available_extensions_count = 0;
        vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, VK_NULL_HANDLE));
        // Create an array with enough room and fetch the available desired_extensions
        Array<VkExtensionProperties> available_extensions(available_extensions_count);
        vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data()));

        // For each desired extension, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_extension : desired_extensions)
        {
            bool       found = false;
            const auto ext   = std::string(desired_extension);

            // Search available extensions until the desired one is found or not
            for (const auto &available_extension : available_extensions)
            {
                if (ext == std::string(available_extension.extensionName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The extension \"" << ext << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    bool check_device_extension_support(const VkPhysicalDevice &physical_device, const Array<const char *> &desired_extensions)
    {
        // Get the number of available desired_extensions
        uint32_t available_extensions_count = 0;
        vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, VK_NULL_HANDLE));
        // Create an array with enough room and fetch the available desired_extensions
        Array<VkExtensionProperties> available_extensions(available_extensions_count);
        vk_check(
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, available_extensions.data()));

        // For each desired extension, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_extension : desired_extensions)
        {
            bool       found = false;
            const auto ext   = std::string(desired_extension);

            // Search available extensions until the desired one is found or not
            for (const auto &available_extension : available_extensions)
            {
                if (ext == std::string(available_extension.extensionName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The extension \"" << ext << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    bool check_layer_support(const Array<const char *> &desired_layers)
    {
        // Get the number of available desired_layers
        uint32_t available_layers_count = 0;
        vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, nullptr));
        // Create an array with enough room and fetch the available desired_layers
        Array<VkLayerProperties> available_layers(available_layers_count);
        vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers.data()));

        // For each desired layer, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_layer : desired_layers)
        {
            bool       found = false;
            const auto layer = std::string(desired_layer);

            // Search available layers until the desired one is found or not
            for (const auto &available_layer : available_layers)
            {
                if (layer == std::string(available_layer.layerName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The layer \"" << layer << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    /**
     * Callback for the vulkan debug messenger
     * @param message_severity Severity of the message
     * @param message_types Type of the message
     * @param callback_data Additional data concerning the message
     * @param user_data User data passed to the debug messenger
     */
    VkBool32 *debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                       VkDebugUtilsMessageTypeFlagsEXT             message_types,
                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data)
    {
        // Inspired by VkBootstrap's default debug messenger. (Made by Charles Giessen)
        // Get severity
        const char *str_severity;
        switch (message_severity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: str_severity = "VERBOSE"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: str_severity = "TF_ERROR"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: str_severity = "TF_WARNING"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: str_severity = "INFO"; break;
            default: str_severity = "UNKNOWN"; break;
        }

        // Get type
        const char *str_type;
        switch (message_types)
        {
            case 7: str_type = "General | Validation | Performance"; break;
            case 6: str_type = "Validation | Performance"; break;
            case 5: str_type = "General | Performance"; break;
            case 4: str_type = "Performance"; break;
            case 3: str_type = "General | Validation"; break;
            case 2: str_type = "Validation"; break;
            case 1: str_type = "General"; break;
            default: str_type = "Unknown"; break;
        }

        // Print the message to stderr if it is an error.
        auto &output = message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? std::cerr : std::cout;
        output << "[" << str_severity << ": " << str_type << "]\n" << callback_data->pMessage << "\n";

        return VK_FALSE;
    }

    // endregion

    // region Physical device functions

    /**
     * @brief Computes a score for the given physical device.
     * @param device is the device to evaluate.
     * @return the score of that device. A bigger score means that the device is better suited.
     */
    uint32_t rg_renderer_rate_physical_device(const VkPhysicalDevice &device)
    {
        uint32_t score = 0;

        // Get properties and features of that device
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures   device_features;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);

        // Prefer discrete gpu when available
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 10000;
        }

        // The bigger, the better
        score += device_properties.limits.maxImageDimension2D;

        // The device needs to support the following device extensions, otherwise it is unusable
        Array<const char *> required_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        bool extensions_are_supported = check_device_extension_support(device, required_device_extensions);

        // Reset score if the extension are not supported because it is mandatory
        if (!extensions_are_supported)
        {
            score = 0;
        }

        std::cout << "GPU: " << device_properties.deviceName << " | Score: " << score << "\n";

        return score;
    }

    // endregion

    // ---==== Renderer ====---

    Renderer::Renderer(const Window  &example_window,
                       const char    *application_name,
                       const Version &application_version,
                       uint32_t       window_capacity)
        : m_data(new Data)
    {
        // Initialize volk
        vk_check(volkInitialize(), "Couldn't initialize Volk.");

        // --=== Instance creation ===--

        // region Instance creation
        // Do it in a sub scope to call destructors earlier
        {
            // Set required extensions
            uint32_t extra_extension_count = 0;
#ifdef USE_VK_VALIDATION_LAYERS
            extra_extension_count += 1;
#endif
            // Get the extensions that the window manager needs
            auto required_extensions = example_window.get_required_vulkan_extensions(extra_extension_count);

            // Add other extensions in the extra slots
            auto extra_ext_index = required_extensions.count() - extra_extension_count;
#ifdef USE_VK_VALIDATION_LAYERS
            required_extensions[extra_ext_index++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

            check(check_instance_extension_support(required_extensions), "Not all required Vulkan extensions are supported.");

            // Get the validation layers if needed
#ifdef USE_VK_VALIDATION_LAYERS
#define ENABLED_LAYERS_COUNT 1
            Array<const char *> enabled_layers(ENABLED_LAYERS_COUNT);
            enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
            check(check_layer_support(enabled_layers), "Vulkan validation layers requested, but not available.");
#endif

            VkApplicationInfo applicationInfo = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = VK_NULL_HANDLE,
                // Application infos
                .pApplicationName   = application_name,
                .applicationVersion = VK_MAKE_VERSION(application_version.major, application_version.minor, application_version.patch),
                // Engine infos
                .pEngineName   = "Railguard",
                .engineVersion = VK_MAKE_VERSION(ENGINE_VERSION.major, ENGINE_VERSION.minor, ENGINE_VERSION.patch),
                .apiVersion    = VULKAN_API_VERSION,
            };

            VkInstanceCreateInfo instanceCreateInfo {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                // App info
                .pApplicationInfo = &applicationInfo,
            // Validation layers
#ifdef USE_VK_VALIDATION_LAYERS
                .enabledLayerCount   = ENABLED_LAYERS_COUNT,
                .ppEnabledLayerNames = enabled_layers.data(),
#else
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
#endif
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_extensions.count()),
                .ppEnabledExtensionNames = required_extensions.data(),
            };

            vk_check(vkCreateInstance(&instanceCreateInfo, nullptr, &m_data->instance), "Couldn't create instance.");

            // Register instance in Volk
            volkLoadInstance(m_data->instance);

            // Create debug messenger
#ifdef USE_VK_VALIDATION_LAYERS
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
                // Struct info
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = VK_NULL_HANDLE,
                // Message settings
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                // Callback
                .pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT) debug_messenger_callback,
            };
            vk_check(vkCreateDebugUtilsMessengerEXT(m_data->instance, &debug_messenger_create_info, nullptr, &m_data->debug_messenger),
                     "Couldn't create debug messenger");
#endif
        }
        // endregion

        // --=== Physical device and queue families selection ===--

        // region Physical device and queue families selection

        {
            // Get the number of available devices
            uint32_t available_physical_devices_count = 0;
            vkEnumeratePhysicalDevices(m_data->instance, &available_physical_devices_count, nullptr);

            // Create an array big enough to hold everything and get the devices themselves
            Array<VkPhysicalDevice> available_physical_devices(available_physical_devices_count);
            vkEnumeratePhysicalDevices(m_data->instance, &available_physical_devices_count, available_physical_devices.data());

            // Find the best physical device
            // For that, we will assign each device a score and keep the best one
            uint32_t current_max_score = 0;
            for (uint32_t i = 0; i < available_physical_devices_count; i++)
            {
                const VkPhysicalDevice &checked_device = available_physical_devices[i];
                uint32_t                score          = rg_renderer_rate_physical_device(checked_device);

                if (score > current_max_score)
                {
                    // New best device found, save it.
                    // We don't need to keep the previous one, since we definitely won't choose it.
                    current_max_score       = score;
                    m_data->physical_device = checked_device;
                }
            }

            // There is a problem if the device is still null: it means none was found.
            check(m_data->physical_device != VK_NULL_HANDLE, "No suitable GPU was found.");

            // Log chosen GPU
            VkPhysicalDeviceProperties physical_device_properties;
            vkGetPhysicalDeviceProperties(m_data->physical_device, &physical_device_properties);
            printf("Suitable GPU found: %s\n", physical_device_properties.deviceName);

            // Get queue families
            uint32_t queue_family_properties_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device, &queue_family_properties_count, VK_NULL_HANDLE);
            Array<VkQueueFamilyProperties> queue_family_properties(queue_family_properties_count);
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device,
                                                     &queue_family_properties_count,
                                                     queue_family_properties.data());

            // Find the queue families that we need
            bool found_graphics_queue = false;

            for (uint32_t i = 0; i < queue_family_properties_count; i++)
            {
                const auto &family_properties = queue_family_properties[i];

                // Save the graphics queue family_index
                if (family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_data->graphics_queue.family_index = i;
                    found_graphics_queue                = true;

                    break;
                }
            }

            // If we didn't find a graphics queue, we can't continue
            check(found_graphics_queue, "Unable to find a graphics queue family_index.");
        }

        // endregion

        // --=== Logical device and queues creation ===--

        // region Device and queues creation

        { // Define the parameters for the graphics queue
            float                   graphics_queue_priority    = 1.0f;
            VkDeviceQueueCreateInfo graphics_queue_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                // Queue info
                .queueFamilyIndex = m_data->graphics_queue.family_index,
                .queueCount       = 1,
                .pQueuePriorities = &graphics_queue_priority,
            };
            Array<const char *> required_device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            // Create the logical device
            VkDeviceCreateInfo device_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                // Queue infos
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos    = &graphics_queue_create_info,
                // Layers
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.count()),
                .ppEnabledExtensionNames = required_device_extensions.data(),
                .pEnabledFeatures        = nullptr,
            };
            vk_check(vkCreateDevice(m_data->physical_device, &device_create_info, nullptr, &m_data->device),
                     "Couldn't create logical device.");

            // Load device in volk
            volkLoadDevice(m_data->device);

            // Get created queues
            vkGetDeviceQueue(m_data->device, m_data->graphics_queue.family_index, 0, &m_data->graphics_queue.queue);
        }

        // endregion


    }

    Renderer::Renderer(Renderer &&other) noexcept : m_data(other.m_data)
    {
        // Just take the other's data and set the original to null, so it can't access it anymore
        other.m_data = nullptr;
    }

} // namespace rg
#endif
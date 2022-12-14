#include "Device.hpp"

#include <GLFW/glfw3.h>

#include "Swapchain.hpp"

#include "Utilities.hpp"

#include <stdexcept>
#include <string>
#include <array>
#include <unordered_set>

namespace VE
{
    Device::Device(Window* window)
        : m_Window(window), m_Instance(VK_NULL_HANDLE), m_PhysicalDevice(VK_NULL_HANDLE),
            m_LogicalDevice(VK_NULL_HANDLE), m_GraphicsQueue(VK_NULL_HANDLE), m_PresentQueue(VK_NULL_HANDLE),
            m_Surface(VK_NULL_HANDLE), m_CommandPool(VK_NULL_HANDLE)
    {
        m_ValidationLayers =
        {
                "VK_LAYER_KHRONOS_validation"
        };

        m_DeviceExtensions =
        {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        CreateInstance();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateCommandPool();
    }

    Device::~Device()
    {
        Clean();
    }

    void Device::CreateInstance()
    {
        VkApplicationInfo info{};
        info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        info.pApplicationName = "Vulkan Application";
        info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        info.pEngineName = "Application";
        info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &info;

        std::vector<const char*> extensions =
        {
                "VK_KHR_get_physical_device_properties2"
        };

        uint32_t glfwExtensionCount{};
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for(size_t i = 0; i < static_cast<uint32_t>(glfwExtensionCount); i++)
        {
            extensions.push_back(glfwExtensions[i]);
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = 0;

        if(CheckValidationLayers(m_ValidationLayers))
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance))
    }

    bool Device::CheckValidationLayers(const std::vector<const char*>& layers)
    {
#ifndef NDEBUG
        uint32_t layerCount{};
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for(const char* layerName : layers)
        {
            bool layerFound = false;
            for(const auto& layerProp : availableLayers)
            {
                if(strcmp(layerProp.layerName, layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if(!layerFound)
            {
                throw std::runtime_error("Error: Layer \"" + std::string(layerName) + " not found!");
            }
        }

        return true;
#else
        return false;
#endif
    }

    void Device::PickPhysicalDevice()
    {
        uint32_t deviceCount{};
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if(deviceCount == 0)
        {
            throw std::runtime_error("Error: Could not find GPUs that support vulkan!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for(const auto device : devices)
        {
            if(IsDeviceSuitable(device))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if(m_PhysicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Error: Failed to find suitable GPU!");
        }
    }

    bool Device::IsDeviceSuitable(VkPhysicalDevice physicalDevice)
    {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice);
        bool extensionsSupported = CheckDeviceExtensionSupport(physicalDevice);
        bool swapchainAdequate{};

        if(extensionsSupported)
        {
            SwapchainSupportDetails details = QuerySwapchainSupport(physicalDevice);
            swapchainAdequate = !details.formats.empty() && !details.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

        return queueFamilyIndices.IsComplete() && extensionsSupported && swapchainAdequate && supportedFeatures.samplerAnisotropy;
    }

    bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
    {
        uint32_t extensionCount{};
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

        std::unordered_set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

        for(const auto& ext : extensions)
        {
            requiredExtensions.erase(ext.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device) const
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for(size_t i = 0; i < queueFamilies.size(); i++)
        {
            if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = static_cast<uint32_t>(i);
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), m_Surface, &presentSupport);

            if(presentSupport)
            {
                indices.presentFamily = static_cast<uint32_t>(i);
            }

            if(indices.IsComplete())
            {
                break;
            }
        }

        return indices;
    }

    void Device::CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::unordered_set<uint32_t> queueFamilies =
        {
                indices.graphicsFamily.value(),
                indices.presentFamily.value()
        };

        float queuePriorities = 1.0f;
        for(size_t i{}; const uint32_t family : queueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = family;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriorities;
            queueCreateInfos.push_back(queueCreateInfo);
            i++;
        }

        VkPhysicalDeviceFeatures deviceFeatures{}; // No features just yet
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        uint32_t extensionCount{};
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, extensions.data());

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

#ifndef NDEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
#endif

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_LogicalDevice))

        vkGetDeviceQueue(m_LogicalDevice, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_LogicalDevice, indices.presentFamily.value(), 0, &m_PresentQueue);
    }

    void Device::CreateSurface()
    {
        if(glfwCreateWindowSurface(m_Instance, m_Window->GetGLFWWindow(), nullptr, &m_Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Error: Failed to create surface!");
        }
    }

    void Device::CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_CommandPool))
    }

    SwapchainSupportDetails Device::QuerySwapchainSupport(VkPhysicalDevice physicalDevice) const
    {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);
        if(formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, details.presentModes.data());

        return details;
    }

    VkCommandBuffer Device::BeginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void Device::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);

        vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool, 1, &commandBuffer);
    }

    void Device::CreateDescriptorLayouts()
    {
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> layoutBindings;
        layoutBindings.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < layoutBindings.size(); i++)
        {
            uint32_t numDescUniforms = NUM_UNIFORMS;
            uint32_t numDescSamplers = NUM_SAMPLERS;
            uint32_t totalDescriptors = numDescUniforms + numDescSamplers;

            layoutBindings[i].resize(totalDescriptors);
            size_t k = 0;
            for (size_t j = 0; j < numDescUniforms; j++)
            {
                layoutBindings[i][k].binding = static_cast<uint32_t>(k);
                layoutBindings[i][k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                layoutBindings[i][k].descriptorCount = 1;
                layoutBindings[i][k].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                layoutBindings[i][k].pImmutableSamplers = nullptr;
                k++;
            }

            for (size_t j = 0; j < numDescSamplers; j++)
            {
                layoutBindings[i][k].binding = static_cast<uint32_t>(k);
                layoutBindings[i][k].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                layoutBindings[i][k].descriptorCount = 1;
                layoutBindings[i][k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                layoutBindings[i][k].pImmutableSamplers = nullptr;
                k++;
            }
        }

        m_DescriptorSetLayouts.resize(layoutBindings.size());
        for (size_t i = 0; i < m_DescriptorSetLayouts.size(); i++)
        {
            VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
            layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings[i].size());
            layoutCreateInfo.pBindings = layoutBindings[i].data();

            VK_CHECK(vkCreateDescriptorSetLayout(m_LogicalDevice, &layoutCreateInfo, nullptr, &m_DescriptorSetLayouts[i]))
        }
    }

    void Device::CreateDescriptorPool(const uint32_t numObjects)
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes;

        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1 * numObjects;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1 * numObjects;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 2 * numObjects;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes = poolSizes.data();

        VK_CHECK(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_DescriptorPool))
    }

    uint32_t Device::FindMemoryType(Device* device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            // typeFilter logical and checks if this memory type can be used
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("Error: Failed to find suitable memory type!");
    }

    void Device::Clean()
    {
        if (!m_DescriptorSetLayouts.empty())
        {
            for (const auto& layout : m_DescriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(m_LogicalDevice, layout, nullptr);
            }
        }
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, nullptr);
        }
        if (m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
        }
        if(m_LogicalDevice != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_LogicalDevice, nullptr);
        }
        if(m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        }
        if(m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
        }
    }
}

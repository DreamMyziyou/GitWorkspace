//
// Created by WeslyChen on 2024/1/21.
//

#include "VulkanDeviceSwapChainWrapper.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <vector>

#include "Engine/EngineCore.h"
#include "VulkanManager.h"

using namespace std;

static const vector<const char*> gDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

void VulkanDeviceSwapChainWrapper::Create()
{
    auto vulkanInstance = VulkanManager::instance()->GetVulkanInstance();
    auto surface = VulkanManager::instance()->GetSurface();
    if (!vulkanInstance || !surface)
        return;

    CreatePhysicalDevice();
    CreateLogicDevice();
    CreateSwapChain();
    CreateImageViews();
}

void VulkanDeviceSwapChainWrapper::Destroy()
{
    if (!mLogicDevice)
        return;

    for (auto imageView : mSwapChainImageViews)
        vkDestroyImageView(mLogicDevice, imageView, nullptr);

    mSwapChainImages.clear();
    vkDestroySwapchainKHR(mLogicDevice, mSwapChain, nullptr);
    mSwapChain = nullptr;

    vkDestroyDevice(mLogicDevice, nullptr);
    mLogicDevice = nullptr;
}

bool VulkanDeviceSwapChainWrapper::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    set<String> requiredExtensions(gDeviceExtensions.begin(), gDeviceExtensions.end());

    for (const auto& extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();
}

bool VulkanDeviceSwapChainWrapper::IsDeviceSuitable(VkPhysicalDevice device) const
{
    if (!device)
        return false;

    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete())
        return false;

    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    if (!extensionsSupported)
        return false;

    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

    if (!swapChainAdequate)
        return false;

    return true;
}

int32 VulkanDeviceSwapChainWrapper::ScoreDeviceSuitability(VkPhysicalDevice device) const
{
    if (!IsDeviceSuitable(device))
        return 0;

    int32 score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // Application can't function without geometry shaders
    if (!deviceFeatures.geometryShader)
        return 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

VkSurfaceFormatKHR VulkanDeviceSwapChainWrapper::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return availableFormat;
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanDeviceSwapChainWrapper::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanDeviceSwapChainWrapper::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, VulkanManager::instance()->GetSurface(), &surfaceCapabilities);

    VkExtent2D actualExtent = surfaceCapabilities.currentExtent;

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

VulkanDeviceSwapChainWrapper::QueueFamilyIndices VulkanDeviceSwapChainWrapper::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int32 i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (indices.IsComplete())
            break;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, VulkanManager::instance()->GetSurface(), &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        i++;
    }

    return indices;
}

VulkanDeviceSwapChainWrapper::SwapChainSupportDetails VulkanDeviceSwapChainWrapper::QuerySwapChainSupport(VkPhysicalDevice device) const
{
    SwapChainSupportDetails details;

    auto surface = VulkanManager::instance()->GetSurface();
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void VulkanDeviceSwapChainWrapper::CreatePhysicalDevice()
{
    // Get device
    uint32 deviceCount = 0;
    vkEnumeratePhysicalDevices(VulkanManager::instance()->GetVulkanInstance(), &deviceCount, nullptr);
    if (deviceCount == 0)
        return Logger::LogFatal("VulkanRender", "Failed to find GPUs with Vulkan support!");
    vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(VulkanManager::instance()->GetVulkanInstance(), &deviceCount, devices.data());

    // Use an ordered map to automatically sort candidates by increasing score
    multimap<int32, VkPhysicalDevice> candidates;
    for (const auto& device : devices)
    {
        int32 score = ScoreDeviceSuitability(device);
        candidates.insert(make_pair(score, device));
    }

    // Check if the best candidate is suitable at all
    if (candidates.rbegin()->first > 0)
        mPhysicalDevice = candidates.rbegin()->second;
    else
        return Logger::LogFatal("VulkanRender", "Failed to find a suitable GPU!");
}

void VulkanDeviceSwapChainWrapper::CreateLogicDevice()
{
    if (!mPhysicalDevice)
        return;

    QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = gDeviceExtensions.data();

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mLogicDevice) != VK_SUCCESS)
        return Logger::LogFatal("VulkanRender", "Failed to create logical device!");

    vkGetDeviceQueue(mLogicDevice, indices.presentFamily.value(), 0, &mGraphicsQueue);
}

void VulkanDeviceSwapChainWrapper::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice);
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = VulkanManager::instance()->GetSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;  // Optional
        createInfo.pQueueFamilyIndices = nullptr;  // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain;
    if (vkCreateSwapchainKHR(mLogicDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        return Logger::LogFatal("VulkanRender", "failed to create swap chain!");

    mSwapChain = swapChain;

    vkGetSwapchainImagesKHR(mLogicDevice, swapChain, &imageCount, nullptr);
    mSwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mLogicDevice, swapChain, &imageCount, mSwapChainImages.data());
    mSwapChainImageFormat = surfaceFormat.format;
    mSwapChainExtent = extent;
}

void VulkanDeviceSwapChainWrapper::CreateImageViews()
{
    if (mSwapChainImages.empty())
        return;

    mSwapChainImageViews.resize(mSwapChainImages.size());

    for (size_t i = 0; i < mSwapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = mSwapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = mSwapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(mLogicDevice, &createInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
            Logger::LogFatal("VulkanRender", "failed to create image views!");
    }
}
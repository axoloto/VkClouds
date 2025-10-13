#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

#include "VkInstance.hpp"

namespace vk
{
struct QueueFamilyIndices
{
  std::optional<uint32_t> graphicsAndComputeFamily; // drawing commands to render an image
  std::optional<uint32_t> presentFamily; // presenting images to the window surface

  bool isComplete()
  {
    return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

const std::vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

class PhysicalDevice
{
  public:
  PhysicalDevice(const VkInstance& instance, VkSurfaceKHR surface);
  ~PhysicalDevice();
  VkPhysicalDevice getVk() const { return m_physicalDevice; }
  QueueFamilyIndices GetQueueFamilyIndices() const { return m_queueFamilyIndices; }
  VkSampleCountFlagBits GetMsaaSamples() const { return m_msaaSamples; }
  SwapChainSupportDetails GetSwapChainSupportDetails() const { return m_swapChainSupportDetails; }
  VkFormat getDepthFormat() const { return m_depthFormat; }
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

  private:
  VkPhysicalDevice m_physicalDevice;
  QueueFamilyIndices m_queueFamilyIndices;
  VkSampleCountFlagBits m_msaaSamples;
  SwapChainSupportDetails m_swapChainSupportDetails;
  VkFormat m_depthFormat;

  VkFormat findDepthFormat();
};
}
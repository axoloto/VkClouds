#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

#include "VkDevice.hpp"
#include "VkPhysicalDevice.hpp"

namespace vk
{
class SwapChain
{
  public:
  SwapChain(const PhysicalDevice& physicalDevice, const Device& device, VkSurfaceKHR surface, int width, int height);
  ~SwapChain();

  VkSwapchainKHR GetVk() const { return m_swapChain; }

  const std::vector<VkImage>& GetImages() const { return m_swapChainImages; }
  const std::vector<VkImageView>& GetImageViews() const { return m_swapChainImageViews; }
  const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_swapChainFramebuffers; }
  VkFramebuffer GetFramebuffer(int index) { return m_swapChainFramebuffers[index]; }
  VkFormat GetFormat() const { return m_swapChainImageFormat; }
  VkExtent2D GetExtent() const { return m_swapChainExtent; }
  uint32_t GetMinImageCount() const { return m_minImageCount; }
  void recreate();
  void cleanup();

  void createFramebuffers(const VkRenderPass& renderPass);

  private:
  VkSwapchainKHR m_swapChain;

  Device m_device;
  VkFormat m_swapChainImageFormat;
  VkExtent2D m_swapChainExtent;
  std::vector<VkImage> m_swapChainImages;
  // An image view is needed to start using image as a texture
  std::vector<VkImageView> m_swapChainImageViews;
  std::vector<VkFramebuffer> m_swapChainFramebuffers;

  uint32_t m_minImageCount;

  VkImage m_depthImage;
  VkDeviceMemory m_depthImageMemory;
  VkImageView m_depthImageView;
  VkImage m_colorImage;
  VkDeviceMemory m_colorImageMemory;
  VkImageView m_colorImageView;

  void createImageViews();
  void createColorResources(VkSampleCountFlagBits msaaSamples);
  void createDepthResources(VkSampleCountFlagBits msaaSamples, VkFormat depthFormat);
};
}
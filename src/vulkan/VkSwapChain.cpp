#include "VkSwapChain.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace vk
{
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
  for (const auto& availableFormat : availableFormats)
  {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
  for (const auto& availablePresentMode : availablePresentModes)
  {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    VkExtent2D actualExtent = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
  }
}

SwapChain::SwapChain(const PhysicalDevice& physicalDevice, const Device& device, VkSurfaceKHR surface, int width, int height)
    : m_device(device)
{
  auto swapChainSupport = physicalDevice.GetSwapChainSupportDetails();

  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
  {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  m_minImageCount = imageCount;

  VkSwapchainCreateInfoKHR createInfo {};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = m_minImageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT if final blit

  auto indices = physicalDevice.GetQueueFamilyIndices();
  uint32_t queueFamilyIndices[] = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

  // Here we are drawing on the images in the swap chain from the gfx queue
  // then submitting them on the presentation queue
  if (indices.graphicsAndComputeFamily != indices.presentFamily)
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  }
  else
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_device.GetVk(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create the swap chain!");
  }

  vkGetSwapchainImagesKHR(m_device.GetVk(), m_swapChain, &imageCount, nullptr);
  m_swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_device.GetVk(), m_swapChain, &imageCount, m_swapChainImages.data());

  m_swapChainImageFormat = surfaceFormat.format;
  m_swapChainExtent = extent;

  createImageViews();
  createColorResources(physicalDevice.GetMsaaSamples());
  createDepthResources(physicalDevice.GetMsaaSamples(), physicalDevice.getDepthFormat());
}

SwapChain::~SwapChain()
{
}

void SwapChain::createImageViews()
{
  m_swapChainImageViews.resize(m_swapChainImages.size());

  for (size_t i = 0; i < m_swapChainImages.size(); ++i)
  {
    m_swapChainImageViews[i] = m_device.createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
  }
}

void SwapChain::createColorResources(VkSampleCountFlagBits msaaSamples)
{
  VkFormat colorFormat = m_swapChainImageFormat;

  m_device.createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, msaaSamples, colorFormat,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_colorImage, m_colorImageMemory);

  m_colorImageView = m_device.createImageView(m_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void SwapChain::createDepthResources(VkSampleCountFlagBits msaaSamples, VkFormat depthFormat)
{
  m_device.createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, msaaSamples, depthFormat,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

  m_depthImageView = m_device.createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

  // optional
  m_device.transitionImageLayout(m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

void SwapChain::createFramebuffers(const VkRenderPass& renderPass)
{
  m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

  for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
  {
    std::array<VkImageView, 3> attachments = {
      m_colorImageView,
      m_depthImageView,
      m_swapChainImageViews[i]
    };

    VkFramebufferCreateInfo framebufferInfo {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_swapChainExtent.width;
    framebufferInfo.height = m_swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_device.GetVk(), &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create framebufer!");
    }
  }
}

void SwapChain::recreate()
{
  //vkDeviceWaitIdle(m_device);

  //cleanup();
  //TODO
  /*
  createSwapChain();
  createImageViews();
  createColorResources();
  createDepthResources();
  createFramebuffers();
  */
}

void SwapChain::cleanup()
{
  auto vkDevice = m_device.GetVk();

  vkDestroyImageView(vkDevice, m_colorImageView, nullptr);
  vkDestroyImage(vkDevice, m_colorImage, nullptr);
  vkFreeMemory(vkDevice, m_colorImageMemory, nullptr);

  vkDestroyImageView(vkDevice, m_depthImageView, nullptr);
  vkDestroyImage(vkDevice, m_depthImage, nullptr);
  vkFreeMemory(vkDevice, m_depthImageMemory, nullptr);

  for (auto framebuffer : m_swapChainFramebuffers)
  {
    vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);
  }

  for (auto imageView : m_swapChainImageViews)
  {
    vkDestroyImageView(vkDevice, imageView, nullptr);
  }

  vkDestroySwapchainKHR(vkDevice, m_swapChain, nullptr);
}
}
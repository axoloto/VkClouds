#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

#include "VkPhysicalDevice.hpp"

namespace vk
{
using PipelineData = std::pair<VkPipeline, VkPipelineLayout>;

class Device
{
  public:
  Device(const PhysicalDevice& physicalDevice);
  ~Device();

  VkDevice GetVk() const { return m_device; }

  VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
  VkQueue GetComputeQueue() const { return m_computeQueue; }
  VkQueue GetPresentQueue() const { return m_presentQueue; }
  uint32_t GetGraphicsAndComputeQueueFamily() const { return m_graphicsAndComputeQueueFamily; }

  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
      VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
      VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

  VkShaderModule createShaderModule(const std::string& shaderFileName);

  PipelineData createComputePipeline(const std::string& shaderFileName, const std::string& shaderPassName, VkDescriptorSetLayout* descSetLayout);

  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommandsAndSubmitOnGraphicsQueue(VkCommandBuffer commandBuffer);

  void transitionImageLayout(VkImage image, VkFormat format,
      VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

  VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }

  VkCommandBuffer getGraphicsCommandBuffer(int index) const { return m_graphicsCommandBuffers[index]; }
  VkCommandBuffer getComputeCommandBuffer(int index) const { return m_computeCommandBuffers[index]; }

  private:
  VkDevice m_device;
  uint32_t m_graphicsAndComputeQueueFamily;
  VkQueue m_graphicsQueue;
  VkQueue m_computeQueue;
  VkQueue m_presentQueue;
  VkCommandPool m_commandPool;
  VkDescriptorPool m_descriptorPool;
  std::vector<VkCommandBuffer> m_graphicsCommandBuffers;
  std::vector<VkCommandBuffer> m_computeCommandBuffers;

  PhysicalDevice m_physicalDevice;

  void createCommandPool(const QueueFamilyIndices& queueFamilyIndices);
  void createDescriptorPool();
  void createCommandBuffers();
};
}
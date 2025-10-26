#pragma once

#include "Utils.hpp"
#include "VkDevice.hpp"
#include "VkPhysicalDevice.hpp"
#include "VkSwapChain.hpp"
#include <array>
#include <functional>

class GraphicsEngine
{
  public:
  GraphicsEngine(vk::SwapChain* swapChain,
      vk::PhysicalDevice* physicalDevice,
      vk::Device* device,
      std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> particleShaderStorageBuffers,
      std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> startEndParticleIdShaderStorageBuffers,
      std::function<void(VkCommandBuffer)> drawUIFunc);
  ~GraphicsEngine();
  VkRenderPass renderPass() { return m_renderPass; }
  void draw(size_t currentFrame, VkSemaphore computeFinishedSemaphore);

  private:
  // owned by app
  vk::PhysicalDevice* m_physicalDevice;
  vk::Device* m_device;
  vk::SwapChain* m_swapChain;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_particleSSBOs;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_startEndParticleIdSSBOs;
  std::function<void(VkCommandBuffer)> m_drawUIFunc;

  void createRenderPass();
  void createGraphicsDescriptorSetLayout();
  void createBoxGraphicsPipeline();
  void createGridGraphicsPipeline();
  void createParticleGraphicsPipeline();
  void createBoxVertexBuffer();
  void createBoxIndexBuffer();
  void createGridVertexBuffer();
  void createGridIndexBuffer();
  void createGraphicsDescriptorSets();
  void createUniformBuffers();
  void createSyncObjects();

  void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, size_t currentFrame);
  void updateUniformBuffer(uint32_t currentImage);

  // owned by graphics engine
  VkDescriptorSetLayout graphicsDescriptorSetLayout;
  std::vector<VkDescriptorSet> graphicsDescriptorSets;

  VkRenderPass m_renderPass;

  VkPipelineLayout boxGraphicsPipelineLayout;
  VkPipeline boxGraphicsPipeline;

  VkPipelineLayout gridGraphicsPipelineLayout;
  VkPipeline gridGraphicsPipeline;

  VkPipelineLayout particleGraphicsPipelineLayout;
  VkPipeline particleGraphicsPipeline;

  VkBuffer boxVertexBuffer;
  VkDeviceMemory boxVertexBufferMemory;
  VkBuffer boxIndexBuffer;
  VkDeviceMemory boxIndexBufferMemory;

  VkBuffer gridVertexBuffer;
  VkDeviceMemory gridVertexBufferMemory;
  VkBuffer gridIndexBuffer;
  VkDeviceMemory gridIndexBufferMemory;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;
};
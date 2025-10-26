#pragma once

#include "Utils.hpp"
#include "VkDevice.hpp"
#include "VkPhysicalDevice.hpp"
#include <array>

class PhysicsEngine
{
  public:
  PhysicsEngine(vk::Device* device,
      std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> particleShaderStorageBuffers,
      std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> startEndParticleIdShaderStorageBuffers);
  ~PhysicsEngine();
  void simulate(size_t currentFrame);
  VkSemaphore computeFinishedSemaphore(size_t currentFrame) { return computeFinishedSemaphores[currentFrame]; }

  private:
  // owned by app
  vk::Device* m_device;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_particleSSBOs;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> m_startEndParticleIdSSBOs;

  // owned by physics engine
  VkDescriptorSetLayout boidsComputeDescriptorSetLayout;
  VkDescriptorSetLayout cellIdComputeDescriptorSetLayout;

  std::vector<VkDescriptorSet> boidsComputeDescriptorSets;
  std::vector<VkDescriptorSet> cellIdComputeDescriptorSets;

  vk::PipelineData boidsCompPipeline;
  vk::PipelineData resetStartEndPartIdCompPipeline;
  vk::PipelineData fillStartEndPartIdCompPipeline;
  vk::PipelineData adjustEndPartIdCompPipeline;
  vk::PipelineData fillCellIdsCompPipeline;

  std::vector<VkFence> computeInFlightFences;
  std::vector<VkSemaphore> computeFinishedSemaphores;
  std::vector<VkBuffer> computeUniformBuffers;
  std::vector<VkDeviceMemory> computeUniformBuffersMemory;
  std::vector<void*> computeUniformBuffersMapped;

  void createBoidsComputeDescriptorSetLayout();
  void createCellIdComputeDescriptorSetLayout();
  void createBoidsComputeDescriptorSets();
  void createCellIdComputeDescriptorSets();
  void createUniformBuffers();
  void createSyncObjects();
  void updateComputeUniformBuffer(uint32_t currentImage);
  void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame);
};
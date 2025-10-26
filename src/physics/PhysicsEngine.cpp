#include "PhysicsEngine.hpp"
#include "Particle.hpp"
#include "Utils.hpp"
#include "Vertex.hpp"

#include <array>
#include <stdexcept>
#include <vulkan/vulkan.h>

struct ComputeUniformBufferObject
{
  float deltaTime;
};

PhysicsEngine::PhysicsEngine(vk::Device* device,
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> particleShaderStorageBuffers,
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> startEndParticleIdShaderStorageBuffers)
    : m_device(device)
    , m_particleSSBOs(particleShaderStorageBuffers)
    , m_startEndParticleIdSSBOs(startEndParticleIdShaderStorageBuffers)

{
  createBoidsComputeDescriptorSetLayout();
  createCellIdComputeDescriptorSetLayout();

  boidsCompPipeline = m_device->createComputePipeline("boids.spv", "main", &boidsComputeDescriptorSetLayout);
  fillCellIdsCompPipeline = m_device->createComputePipeline("fillCellIds.spv", "main", &cellIdComputeDescriptorSetLayout);
  resetStartEndPartIdCompPipeline = m_device->createComputePipeline("resetStartEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);
  fillStartEndPartIdCompPipeline = m_device->createComputePipeline("fillStartEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);
  adjustEndPartIdCompPipeline = m_device->createComputePipeline("adjustEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);

  createUniformBuffers();
  createBoidsComputeDescriptorSets();
  createCellIdComputeDescriptorSets();
  createSyncObjects();
};

PhysicsEngine::~PhysicsEngine()
{
  vkDestroyDescriptorSetLayout(m_device->GetVk(), boidsComputeDescriptorSetLayout, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(m_device->GetVk(), computeFinishedSemaphores[i], nullptr);
    vkDestroyFence(m_device->GetVk(), computeInFlightFences[i], nullptr);
  }

  vkDestroyPipeline(m_device->GetVk(), boidsCompPipeline.first, nullptr);
  vkDestroyPipelineLayout(m_device->GetVk(), boidsCompPipeline.second, nullptr);
}

void PhysicsEngine::createBoidsComputeDescriptorSetLayout()
{
  VkDescriptorSetLayoutBinding uboLayoutBinding {};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding ssboInLayoutBinding {};
  ssboInLayoutBinding.binding = 1;
  ssboInLayoutBinding.descriptorCount = 1;
  ssboInLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboInLayoutBinding.pImmutableSamplers = nullptr;
  ssboInLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding ssboOutLayoutBinding {};
  ssboOutLayoutBinding.binding = 2;
  ssboOutLayoutBinding.descriptorCount = 1;
  ssboOutLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboOutLayoutBinding.pImmutableSamplers = nullptr;
  ssboOutLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding ssboStartEndParticleIdLayoutBinding {};
  ssboStartEndParticleIdLayoutBinding.binding = 3;
  ssboStartEndParticleIdLayoutBinding.descriptorCount = 1;
  ssboStartEndParticleIdLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboStartEndParticleIdLayoutBinding.pImmutableSamplers = nullptr;
  ssboStartEndParticleIdLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  std::array<VkDescriptorSetLayoutBinding, 4> bindings = { uboLayoutBinding, ssboInLayoutBinding, ssboOutLayoutBinding, ssboStartEndParticleIdLayoutBinding };

  VkDescriptorSetLayoutCreateInfo layoutInfo {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_device->GetVk(), &layoutInfo, nullptr, &boidsComputeDescriptorSetLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create compute descriptor set layout!");
  }
}

void PhysicsEngine::createCellIdComputeDescriptorSetLayout()
{
  VkDescriptorSetLayoutBinding particleSSBO {};
  particleSSBO.binding = 0;
  particleSSBO.descriptorCount = 1;
  particleSSBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  particleSSBO.pImmutableSamplers = nullptr;
  particleSSBO.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding startEndParticleIdSSBO {};
  startEndParticleIdSSBO.binding = 1;
  startEndParticleIdSSBO.descriptorCount = 1;
  startEndParticleIdSSBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  startEndParticleIdSSBO.pImmutableSamplers = nullptr;
  startEndParticleIdSSBO.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = { particleSSBO, startEndParticleIdSSBO };

  VkDescriptorSetLayoutCreateInfo layoutInfo {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_device->GetVk(), &layoutInfo, nullptr, &cellIdComputeDescriptorSetLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create compute descriptor set layout!");
  }
}

void PhysicsEngine::createUniformBuffers()
{
  VkDeviceSize computeBufferSize = sizeof(ComputeUniformBufferObject);

  computeUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  computeUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  computeUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    m_device->createBuffer(computeBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, computeUniformBuffers[i], computeUniformBuffersMemory[i]);

    // persistent mapping for perf optimization as we reused it
    vkMapMemory(m_device->GetVk(), computeUniformBuffersMemory[i], 0, computeBufferSize, 0, &computeUniformBuffersMapped[i]);
  }
}

void PhysicsEngine::createSyncObjects()
{
  computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    if (vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS
        || vkCreateFence(m_device->GetVk(), &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create semaphores!");
    }
  }
}

void PhysicsEngine::updateComputeUniformBuffer(uint32_t currentImage)
{
  ComputeUniformBufferObject ubo {};
  ubo.deltaTime = 0.001f;

  memcpy(computeUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void PhysicsEngine::createBoidsComputeDescriptorSets()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, boidsComputeDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_device->getDescriptorPool();
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  boidsComputeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device->GetVk(), &allocInfo, boidsComputeDescriptorSets.data()))
  {
    throw std::runtime_error("Failed to allocate compute descriptor sets!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    VkDescriptorBufferInfo computeBufferInfo {};
    computeBufferInfo.buffer = computeUniformBuffers[i];
    computeBufferInfo.offset = 0;
    computeBufferInfo.range = sizeof(ComputeUniformBufferObject);

    VkDescriptorBufferInfo storageBufferInfoLastFrame {};
    storageBufferInfoLastFrame.buffer = m_particleSSBOs[(i - 1) % MAX_FRAMES_IN_FLIGHT];
    storageBufferInfoLastFrame.offset = 0;
    storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

    VkDescriptorBufferInfo storageBufferInfoCurrentFrame {};
    storageBufferInfoCurrentFrame.buffer = m_particleSSBOs[i];
    storageBufferInfoCurrentFrame.offset = 0;
    storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

    VkDescriptorBufferInfo storageBufferInfoStartEndParticleId {};
    storageBufferInfoStartEndParticleId.buffer = m_startEndParticleIdSSBOs[i];
    storageBufferInfoStartEndParticleId.offset = 0;
    storageBufferInfoStartEndParticleId.range = sizeof(glm::uvec2) * GRID_SIZE;

    std::array<VkWriteDescriptorSet, 4> descriptorWrites {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = boidsComputeDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &computeBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = boidsComputeDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = boidsComputeDescriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = boidsComputeDescriptorSets[i];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &storageBufferInfoStartEndParticleId;

    vkUpdateDescriptorSets(m_device->GetVk(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

void PhysicsEngine::createCellIdComputeDescriptorSets()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, cellIdComputeDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_device->getDescriptorPool();
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  cellIdComputeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device->GetVk(), &allocInfo, cellIdComputeDescriptorSets.data()))
  {
    throw std::runtime_error("Failed to allocate compute descriptor sets!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    VkDescriptorBufferInfo storageBufferInfoParticle {};
    storageBufferInfoParticle.buffer = m_particleSSBOs[(i - 1) % MAX_FRAMES_IN_FLIGHT];
    storageBufferInfoParticle.offset = 0;
    storageBufferInfoParticle.range = sizeof(Particle) * PARTICLE_COUNT;

    VkDescriptorBufferInfo storageBufferInfoStartEndParticleId {};
    storageBufferInfoStartEndParticleId.buffer = m_startEndParticleIdSSBOs[i];
    storageBufferInfoStartEndParticleId.offset = 0;
    storageBufferInfoStartEndParticleId.range = sizeof(glm::uvec2) * GRID_SIZE;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = cellIdComputeDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &storageBufferInfoParticle;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = cellIdComputeDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &storageBufferInfoStartEndParticleId;

    vkUpdateDescriptorSets(m_device->GetVk(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

void PhysicsEngine::recordComputeCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame)
{
  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;
  beginInfo.pInheritanceInfo = nullptr;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to bein recording the compute command buffer!");
  }

  // TODO: use a vkBufferMemoryBarrier for more granularity and better perf
  VkMemoryBarrier memoryBarrier;
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.pNext = nullptr;
  memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resetStartEndPartIdCompPipeline.second, 0, 1, &cellIdComputeDescriptorSets[currentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, resetStartEndPartIdCompPipeline.first);
  vkCmdDispatch(commandBuffer, GRID_SIZE / 256, 1, 1);

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
  /*
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, fillStartEndPartIdCompPipeline.second, 0, 1, &cellIdComputeDescriptorSets[currentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, fillStartEndPartIdCompPipeline.first);
  vkCmdDispatch(commandBuffer, GRID_SIZE / 256, 1, 1);

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, adjustEndPartIdCompPipeline.second, 0, 1, &cellIdComputeDescriptorSets[currentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, adjustEndPartIdCompPipeline.first);
  vkCmdDispatch(commandBuffer, GRID_SIZE / 256, 1, 1);

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, fillCellIdsCompPipeline.second, 0, 1, &cellIdComputeDescriptorSets[currentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, fillCellIdsCompPipeline.first);
  vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, boidsCompPipeline.second, 0, 1, &boidsComputeDescriptorSets[currentFrame], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, boidsCompPipeline.first);
  vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);
*/
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to record the compute command buffer!");
  }
}

void PhysicsEngine::simulate(size_t currentFrame)
{
  // Compute Submission
  vkWaitForFences(m_device->GetVk(), 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  updateComputeUniformBuffer(currentFrame);

  vkResetFences(m_device->GetVk(), 1, &computeInFlightFences[currentFrame]);

  auto computeCommandBuffer = m_device->getComputeCommandBuffer(currentFrame);

  vkResetCommandBuffer(computeCommandBuffer, 0);

  recordComputeCommandBuffer(computeCommandBuffer, currentFrame);

  VkSubmitInfo computeSubmitInfo {};
  computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  computeSubmitInfo.commandBufferCount = 1;
  computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
  computeSubmitInfo.signalSemaphoreCount = 1;
  computeSubmitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

  if (vkQueueSubmit(m_device->GetComputeQueue(), 1, &computeSubmitInfo, computeInFlightFences[currentFrame]) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to submit compute command buffer!");
  }
}
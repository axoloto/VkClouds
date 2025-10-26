#include "GraphicsEngine.hpp"
#include "Particle.hpp"
#include "Utils.hpp"
#include "Vertex.hpp"
#include "glm/glm.hpp"

#include <array>
#include <chrono>
#include <stdexcept>

struct UniformBufferObject
{
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

GraphicsEngine::GraphicsEngine(vk::SwapChain* swapChain,
    vk::PhysicalDevice* physicalDevice,
    vk::Device* device,
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> particleShaderStorageBuffers,
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> startEndParticleIdShaderStorageBuffers,
    std::function<void(VkCommandBuffer)> drawUIFunc)
    : m_swapChain(swapChain)
    , m_physicalDevice(physicalDevice)
    , m_device(device)
    , m_particleSSBOs(particleShaderStorageBuffers)
    , m_startEndParticleIdSSBOs(startEndParticleIdShaderStorageBuffers)
    , m_drawUIFunc(drawUIFunc)
{
  createRenderPass();

  createGraphicsDescriptorSetLayout();

  createBoxGraphicsPipeline();
  createGridGraphicsPipeline();
  createParticleGraphicsPipeline();

  createBoxVertexBuffer();
  createBoxIndexBuffer();
  createGridVertexBuffer();
  createGridIndexBuffer();

  createUniformBuffers();

  createSyncObjects();

  createGraphicsDescriptorSets();
};

GraphicsEngine::~GraphicsEngine()
{
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroyBuffer(m_device->GetVk(), uniformBuffers[i], nullptr);
    vkFreeMemory(m_device->GetVk(), uniformBuffersMemory[i], nullptr);
  }

  vkDestroyDescriptorSetLayout(m_device->GetVk(), graphicsDescriptorSetLayout, nullptr);

  vkDestroyBuffer(m_device->GetVk(), boxVertexBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), boxVertexBufferMemory, nullptr);

  vkDestroyBuffer(m_device->GetVk(), boxIndexBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), boxIndexBufferMemory, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(m_device->GetVk(), imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(m_device->GetVk(), renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(m_device->GetVk(), inFlightFences[i], nullptr);
  }

  vkDestroyPipeline(m_device->GetVk(), boxGraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_device->GetVk(), boxGraphicsPipelineLayout, nullptr);

  vkDestroyPipeline(m_device->GetVk(), particleGraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_device->GetVk(), particleGraphicsPipelineLayout, nullptr);

  vkDestroyRenderPass(m_device->GetVk(), m_renderPass, nullptr);
}

void GraphicsEngine::createRenderPass()
{
  // color msaa
  VkAttachmentDescription colorAttachment {};
  colorAttachment.format = m_swapChain->GetFormat();
  colorAttachment.samples = m_physicalDevice->GetMsaaSamples();
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // before render pass
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // after render pass, using intermediate color buffer for msaa, can't be presented, need to be resolved first

  VkAttachmentReference colorAttachmentRef {};
  colorAttachmentRef.attachment = 0; // index -> fragment shader layout(location = )
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // color resolve
  VkAttachmentDescription colorResolveAttachment {};
  colorResolveAttachment.format = m_swapChain->GetFormat();
  colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // before render pass
  colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // after render pass

  VkAttachmentReference colorResolveAttachmentRef {};
  colorResolveAttachmentRef.attachment = 2; // index -> fragment shader layout(location = )
  colorResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // depth msaa
  VkAttachmentDescription depthAttachment {};
  depthAttachment.format = m_physicalDevice->getDepthFormat();
  depthAttachment.samples = m_physicalDevice->GetMsaaSamples();
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef {};
  depthAttachmentRef.attachment = 1; // index -> fragment shader layout(location = )
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // subpass
  VkSubpassDescription subpass {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;
  subpass.pResolveAttachments = &colorResolveAttachmentRef;

  // subpass dependency
  VkSubpassDependency dependency {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  // render pass
  std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorResolveAttachment };
  VkRenderPassCreateInfo renderPassInfo {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(m_device->GetVk(), &renderPassInfo, nullptr, &m_renderPass))
  {
    throw std::runtime_error("Failed to create render pass!");
  }
}

void GraphicsEngine::createGraphicsDescriptorSetLayout()
{
  VkDescriptorSetLayoutBinding uboLayoutBinding {};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding ssboStartEndParticleIdLayoutBinding {};
  ssboStartEndParticleIdLayoutBinding.binding = 1;
  ssboStartEndParticleIdLayoutBinding.descriptorCount = 1;
  ssboStartEndParticleIdLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  ssboStartEndParticleIdLayoutBinding.pImmutableSamplers = nullptr;
  ssboStartEndParticleIdLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, ssboStartEndParticleIdLayoutBinding };
  VkDescriptorSetLayoutCreateInfo layoutInfo {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_device->GetVk(), &layoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create graphics descriptor set layout!");
  }
}

void GraphicsEngine::createBoxGraphicsPipeline()
{
  // shaders
  VkShaderModule vertShaderModule = m_device->createShaderModule("boxVert.spv");
  VkShaderModule fragShaderModule = m_device->createShaderModule("boxFrag.spv");

  VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

  // vertex buffer
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  // input assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // dynamic states
  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineDepthStencilStateCreateInfo depthStencil {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // optional
  depthStencil.maxDepthBounds = 1.0f; // optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // optional
  depthStencil.back = {}; // optional

  // viewport
  // region of the framebuffer that the output will be rendered to
  VkViewport viewport {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_swapChain->GetExtent().width;
  viewport.height = (float)m_swapChain->GetExtent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // scissor rectangle - define in which region pixels will stored,
  // any pixel outside will be discarded by the rasterizer
  VkRect2D scissor {};
  scissor.offset = { 0, 0 };
  scissor.extent = m_swapChain->GetExtent();

  // rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;

  // msaa
  VkPipelineMultisampleStateCreateInfo multisampling {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = m_physicalDevice->GetMsaaSamples();
  multisampling.minSampleShading = 1.0f;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable = VK_FALSE;

  // color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  // pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  if (vkCreatePipelineLayout(m_device->GetVk(), &pipelineLayoutInfo, nullptr, &boxGraphicsPipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = boxGraphicsPipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  if (vkCreateGraphicsPipelines(m_device->GetVk(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &boxGraphicsPipeline) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create the graphics pipeline!");
  }

  vkDestroyShaderModule(m_device->GetVk(), fragShaderModule, nullptr);
  vkDestroyShaderModule(m_device->GetVk(), vertShaderModule, nullptr);
}

void GraphicsEngine::createGridGraphicsPipeline()
{
  // shaders
  VkShaderModule vertShaderModule = m_device->createShaderModule("gridVert.spv");
  VkShaderModule fragShaderModule = m_device->createShaderModule("gridFrag.spv");

  VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

  // vertex buffer
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  // input assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // dynamic states
  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineDepthStencilStateCreateInfo depthStencil {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // optional
  depthStencil.maxDepthBounds = 1.0f; // optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // optional
  depthStencil.back = {}; // optional

  // viewport
  // region of the framebuffer that the output will be rendered to
  VkViewport viewport {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_swapChain->GetExtent().width;
  viewport.height = (float)m_swapChain->GetExtent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // scissor rectangle - define in which region pixels will stored,
  // any pixel outside will be discarded by the rasterizer
  VkRect2D scissor {};
  scissor.offset = { 0, 0 };
  scissor.extent = m_swapChain->GetExtent();

  // rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;

  // msaa
  VkPipelineMultisampleStateCreateInfo multisampling {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = m_physicalDevice->GetMsaaSamples();
  multisampling.minSampleShading = 1.0f;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable = VK_FALSE;

  // color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  // pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  if (vkCreatePipelineLayout(m_device->GetVk(), &pipelineLayoutInfo, nullptr, &gridGraphicsPipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = boxGraphicsPipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  if (vkCreateGraphicsPipelines(m_device->GetVk(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gridGraphicsPipeline) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create the graphics pipeline!");
  }

  vkDestroyShaderModule(m_device->GetVk(), fragShaderModule, nullptr);
  vkDestroyShaderModule(m_device->GetVk(), vertShaderModule, nullptr);
}

void GraphicsEngine::createParticleGraphicsPipeline()
{
  // shaders
  VkShaderModule vertShaderModule = m_device->createShaderModule("particleVert.spv");
  VkShaderModule fragShaderModule = m_device->createShaderModule("particleFrag.spv");

  VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

  // vertex buffer

  auto bindingDescription = Particle::getBindingDescription();
  auto attributeDescriptions = Particle::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  // input assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // dynamic states
  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineDepthStencilStateCreateInfo depthStencil {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // optional
  depthStencil.maxDepthBounds = 1.0f; // optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // optional
  depthStencil.back = {}; // optional

  // viewport
  // region of the framebuffer that the output will be rendered to
  VkViewport viewport {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_swapChain->GetExtent().width;
  viewport.height = (float)m_swapChain->GetExtent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // scissor rectangle - define in which region pixels will stored,
  // any pixel outside will be discarded by the rasterizer
  VkRect2D scissor {};
  scissor.offset = { 0, 0 };
  scissor.extent = m_swapChain->GetExtent();

  // rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;

  // msaa
  VkPipelineMultisampleStateCreateInfo multisampling {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = m_physicalDevice->GetMsaaSamples();
  multisampling.minSampleShading = 1.0f;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable = VK_FALSE;

  // color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  // pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  if (vkCreatePipelineLayout(m_device->GetVk(), &pipelineLayoutInfo, nullptr, &particleGraphicsPipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = particleGraphicsPipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  if (vkCreateGraphicsPipelines(m_device->GetVk(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &particleGraphicsPipeline) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create the particle graphics pipeline!");
  }

  vkDestroyShaderModule(m_device->GetVk(), fragShaderModule, nullptr);
  vkDestroyShaderModule(m_device->GetVk(), vertShaderModule, nullptr);
}

void GraphicsEngine::createBoxVertexBuffer()
{
  std::vector<Vertex> vertices;

  int boxLength = 2.0f;
  auto box3DVertices = Geometry::RefCubeVertices;
  for (auto& vertex : box3DVertices)
  {
    float x = vertex[0] * boxLength / 2.0f;
    float y = vertex[1] * boxLength / 2.0f;
    float z = vertex[2] * boxLength / 2.0f;

    Vertex vertex {};

    vertex.pos = { x, y, z };
    vertex.texCoord = { 0.0f, 0.0f }; //not used
    vertex.color = { 1.0f, 1.0f, 1.0f };

    vertices.push_back(vertex);
  }

  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  // Mapping the buffer memory into CPU accessible memory
  // Transfer to GPU is only guaranteed to be complete as of the next call to vkQueueSubmit
  void* data;
  vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, boxVertexBuffer, boxVertexBufferMemory);

  m_device->copyBuffer(stagingBuffer, boxVertexBuffer, bufferSize);

  vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
}

void GraphicsEngine::createBoxIndexBuffer()
{
  auto indices = Geometry::RefCubeIndices;

  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, boxIndexBuffer, boxIndexBufferMemory);

  m_device->copyBuffer(stagingBuffer, boxIndexBuffer, bufferSize);

  vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
}

void GraphicsEngine::createGridVertexBuffer()
{
  Geometry::Vertex3D cellDims;
  cellDims[0] = (float)2.0f / 8.0f;
  cellDims[1] = (float)2.0f / 8.0f;
  cellDims[2] = (float)2.0f / 8.0f;

  auto localCellCoords = Geometry::RefCubeVertices;
  for (auto& vertex : localCellCoords)
  {
    float x = vertex[0] * cellDims[0] * 0.5f;
    float y = vertex[1] * cellDims[1] * 0.5f;
    float z = vertex[2] * cellDims[2] * 0.5f;
    vertex = { x, y, z };
  }

  size_t centerIndex = 0;
  size_t numCells = 8 * 8 * 8;
  Geometry::Vertex3D firstPos;
  firstPos[0] = -(float)2.0f / 2.0f + 0.5f * cellDims[0];
  firstPos[1] = -(float)2.0f / 2.0f + 0.5f * cellDims[1];
  firstPos[2] = -(float)2.0f / 2.0f + 0.5f * cellDims[2];
  std::vector<Geometry::Vertex3D> globalCellCenterCoords(numCells);
  for (int x = 0; x < 8; ++x)
  {
    float xCoord = firstPos[0] + x * cellDims[0];
    for (int y = 0; y < 8; ++y)
    {
      float yCoord = firstPos[1] + y * cellDims[1];
      for (int z = 0; z < 8; ++z)
      {
        float zCoord = firstPos[2] + z * cellDims[2];
        globalCellCenterCoords.at(centerIndex++) = { xCoord, yCoord, zCoord };
      }
    }
  }

  size_t cornerIndex = 0;
  std::vector<Vertex> vertices(numCells * 8);
  for (const auto& centerCoords : globalCellCenterCoords)
  {
    for (const auto& cornerCoords : localCellCoords)
    {
      vertices.at(cornerIndex).pos.x = cornerCoords[0] + centerCoords[0];
      vertices.at(cornerIndex).pos.y = cornerCoords[1] + centerCoords[1];
      vertices.at(cornerIndex).pos.z = cornerCoords[2] + centerCoords[2];
      ++cornerIndex;
    }
  }

  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  // Mapping the buffer memory into CPU accessible memory
  // Transfer to GPU is only guaranteed to be complete as of the next call to vkQueueSubmit
  void* data;
  vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gridVertexBuffer, gridVertexBufferMemory);

  m_device->copyBuffer(stagingBuffer, gridVertexBuffer, bufferSize);

  vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
}

void GraphicsEngine::createGridIndexBuffer()
{
  size_t index = 0;
  uint32_t globalOffset = 0;
  std::vector<uint32_t> indices(8 * 8 * 8 * 24);
  for (int i = 0; i < 512; ++i)
  {
    for (const auto& localIndex : Geometry::RefCubeIndices)
    {
      indices.at(index++) = localIndex + globalOffset;
    }
    globalOffset += 8;
  }

  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

  m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gridIndexBuffer, gridIndexBufferMemory);

  m_device->copyBuffer(stagingBuffer, gridIndexBuffer, bufferSize);

  vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
  vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
}

void GraphicsEngine::createUniformBuffers()
{
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
  uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    m_device->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

    // persistent mapping for perf optimization as we reused it
    vkMapMemory(m_device->GetVk(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
  }
}

void GraphicsEngine::createSyncObjects()
{
  imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    if (vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
        || vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
        || vkCreateFence(m_device->GetVk(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create semaphores!");
    }
  }
}

void GraphicsEngine::createGraphicsDescriptorSets()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, graphicsDescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_device->getDescriptorPool();
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  graphicsDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  if (vkAllocateDescriptorSets(m_device->GetVk(), &allocInfo, graphicsDescriptorSets.data()))
  {
    throw std::runtime_error("Failed to allocate graphics descriptor sets!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = uniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorBufferInfo storageBufferInfoStartEndParticleId {};
    storageBufferInfoStartEndParticleId.buffer = m_startEndParticleIdSSBOs[i];
    storageBufferInfoStartEndParticleId.offset = 0;
    storageBufferInfoStartEndParticleId.range = sizeof(glm::uvec2) * GRID_SIZE;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = graphicsDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = graphicsDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &storageBufferInfoStartEndParticleId;

    vkUpdateDescriptorSets(m_device->GetVk(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

void GraphicsEngine::draw(size_t currentFrame, VkSemaphore computeFinishedSemaphore)
{
  // wait for the previous frame to be finished
  vkWaitForFences(m_device->GetVk(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(m_device->GetVk(), m_swapChain->GetVk(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || false /*framebufferResized*/)
  {
    // framebufferResized = false;
    //recreateSwapChain();
    return;
  }
  else if (result != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to acquire swap chain image!");
  }

  // reset the fence to its unsignaled state
  // only do it if we are submitting work, i.e swap chain is correct
  vkResetFences(m_device->GetVk(), 1, &inFlightFences[currentFrame]);

  auto graphicsCommandBuffer = m_device->getGraphicsCommandBuffer(currentFrame);

  vkResetCommandBuffer(graphicsCommandBuffer, 0);

  updateUniformBuffer(currentFrame);

  recordGraphicsCommandBuffer(graphicsCommandBuffer, imageIndex, currentFrame);

  VkSubmitInfo graphicsSubmitInfo {};
  graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = { computeFinishedSemaphore, imageAvailableSemaphores[currentFrame] };
  // wait for writing colors to the image and read the vertex buffer until it's available
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  graphicsSubmitInfo.waitSemaphoreCount = 2;
  graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
  graphicsSubmitInfo.pWaitDstStageMask = waitStages;

  graphicsSubmitInfo.commandBufferCount = 1;
  graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer;

  VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
  graphicsSubmitInfo.signalSemaphoreCount = 1;
  graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;

  // passing the fence
  if (vkQueueSubmit(m_device->GetGraphicsQueue(), 1, &graphicsSubmitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to submit draw command buffer!");
  }

  // Presentation = submitting the result back to the swapchain
  VkPresentInfoKHR presentInfo {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = { m_swapChain->GetVk() };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pResults = nullptr;

  vkQueuePresentKHR(m_device->GetPresentQueue(), &presentInfo);

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void GraphicsEngine::updateUniformBuffer(uint32_t currentImage)
{
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  UniformBufferObject ubo {};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChain->GetExtent().width / (float)m_swapChain->GetExtent().height, 0.1f, 10.0f);
  ubo.proj[1][1] *= -1;

  memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void GraphicsEngine::recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, size_t currentFrame)
{
  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;
  beginInfo.pInheritanceInfo = nullptr;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to record the graphics command buffer!");
  }

  auto swapChainExtent = m_swapChain->GetExtent();

  VkRenderPassBeginInfo renderPassInfo {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_swapChain->GetFramebuffer(imageIndex);
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = swapChainExtent;

  std::array<VkClearValue, 3> clearValues {};
  clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
  clearValues[1].depthStencil = { 1.0f, 0 };
  clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapChainExtent.width);
  viewport.height = static_cast<float>(swapChainExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor {};
  scissor.offset = { 0, 0 };
  scissor.extent = swapChainExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  VkDeviceSize offsets[] = { 0 };

  // Box rendering
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, boxGraphicsPipeline);

  VkBuffer boxVertexBuffers[] = { boxVertexBuffer };
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, boxVertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, boxIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

  //Binding desc sets only once as they are shared across shaders
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, boxGraphicsPipelineLayout, 0, 1, &graphicsDescriptorSets[currentFrame], 0, nullptr);

  vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(Geometry::RefCubeIndices.size()), 1, 0, 0, 0);

  // Grid rendering
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gridGraphicsPipeline);

  VkBuffer gridVertexBuffers[] = { gridVertexBuffer };
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, gridVertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, gridIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(GRID_SIZE * Geometry::RefCubeIndices.size()), 1, 0, 0, 0);

  // Particles rendering
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particleGraphicsPipeline);

  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_particleSSBOs[currentFrame], offsets);

  vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);

  // UI rendering through DearImgui
  m_drawUIFunc(commandBuffer);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to record the graphics command buffer!");
  }
}

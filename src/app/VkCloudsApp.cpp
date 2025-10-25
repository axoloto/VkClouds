#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include "VkDevice.hpp"
#include "VkInstance.hpp"
#include "VkPhysicalDevice.hpp"
#include "VkSwapChain.hpp"

#include "Geometry.hpp"
#include "Logging.hpp"
#include "Parameters.hpp"
#include "Particle.hpp"
#include "Utils.hpp"
#include "Vertex.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

static constexpr uint32_t WIDTH = 1920;
static constexpr uint32_t HEIGHT = 1080;
static constexpr uint32_t PARTICLE_COUNT = 512;
static constexpr uint32_t GRID_RES = 8;
static constexpr uint32_t GRID_SIZE = GRID_RES * GRID_RES * GRID_RES;

struct UniformBufferObject
{
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct ComputeUniformBufferObject
{
  float deltaTime;
};

static void check_vk_result(VkResult err)
{
  if (err == VK_SUCCESS)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}

class VkCloudsApp
{
  public:
  VkCloudsApp()
  {
    initWindow();
    initVulkan();
    initDearImgui();
  }

  void run()
  {
    mainLoop();
    cleanup();
  }

  private:
  GLFWwindow* window;
  std::unique_ptr<vk::Instance> m_instance;
  std::unique_ptr<vk::PhysicalDevice> m_physicalDevice;
  std::unique_ptr<vk::Device> m_device;
  std::unique_ptr<vk::SwapChain> m_swapChain;

  VkSurfaceKHR surface;

  VkDescriptorSetLayout graphicsDescriptorSetLayout;
  VkDescriptorSetLayout boidsComputeDescriptorSetLayout;
  VkDescriptorSetLayout cellIdComputeDescriptorSetLayout;

  std::vector<VkDescriptorSet> graphicsDescriptorSets;
  std::vector<VkDescriptorSet> boidsComputeDescriptorSets;
  std::vector<VkDescriptorSet> cellIdComputeDescriptorSets;

  VkRenderPass renderPass;

  VkPipelineLayout boxGraphicsPipelineLayout;
  VkPipeline boxGraphicsPipeline;

  VkPipelineLayout gridGraphicsPipelineLayout;
  VkPipeline gridGraphicsPipeline;

  VkPipelineLayout particleGraphicsPipelineLayout;
  VkPipeline particleGraphicsPipeline;

  vk::PipelineData boidsCompPipeline;
  vk::PipelineData resetStartEndPartIdCompPipeline;
  vk::PipelineData fillStartEndPartIdCompPipeline;
  vk::PipelineData adjustEndPartIdCompPipeline;
  vk::PipelineData fillCellIdsCompPipeline;

  VkBuffer boxVertexBuffer;
  VkDeviceMemory boxVertexBufferMemory;
  VkBuffer boxIndexBuffer;
  VkDeviceMemory boxIndexBufferMemory;

  VkBuffer gridVertexBuffer;
  VkDeviceMemory gridVertexBufferMemory;
  VkBuffer gridIndexBuffer;
  VkDeviceMemory gridIndexBufferMemory;

  uint32_t currentFrame = 0;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;
  std::vector<VkBuffer> particleShaderStorageBuffers;
  std::vector<VkDeviceMemory> particleShaderStorageBuffersMemory;
  std::vector<VkBuffer> startEndParticleIdShaderStorageBuffers;
  std::vector<VkDeviceMemory> startEndParticleIdShaderStorageBuffersMemory;

  std::vector<VkFence> computeInFlightFences;
  std::vector<VkSemaphore> computeFinishedSemaphores;
  std::vector<VkBuffer> computeUniformBuffers;
  std::vector<VkDeviceMemory> computeUniformBuffersMemory;
  std::vector<void*> computeUniformBuffersMapped;

  bool framebufferResized = false;

  void initWindow()
  {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "VkClouds", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
  {
    auto app = reinterpret_cast<VkCloudsApp*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
  }

  std::vector<const char*> getRequiredGlfwExtensions()
  {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    return extensions;
  }

  void initVulkan()
  {
    m_instance = std::make_unique<vk::Instance>("VkClouds", getRequiredGlfwExtensions());
    createSurface();
    m_physicalDevice = std::make_unique<vk::PhysicalDevice>(m_instance->getVk(), surface);
    m_device = std::make_unique<vk::Device>(*m_physicalDevice);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    m_swapChain = std::make_unique<vk::SwapChain>(*m_physicalDevice, *m_device, surface, width, height);

    createRenderPass();
    createGraphicsDescriptorSetLayout();
    createBoidsComputeDescriptorSetLayout();
    createCellIdComputeDescriptorSetLayout();
    createBoxGraphicsPipeline();
    createGridGraphicsPipeline();
    createParticleGraphicsPipeline();

    boidsCompPipeline = m_device->createComputePipeline("boids.spv", "main", &boidsComputeDescriptorSetLayout);
    fillCellIdsCompPipeline = m_device->createComputePipeline("fillCellIds.spv", "main", &cellIdComputeDescriptorSetLayout);
    resetStartEndPartIdCompPipeline = m_device->createComputePipeline("resetStartEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);
    fillStartEndPartIdCompPipeline = m_device->createComputePipeline("fillStartEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);
    adjustEndPartIdCompPipeline = m_device->createComputePipeline("adjustEndPartId.spv", "main", &cellIdComputeDescriptorSetLayout);

    m_swapChain->createFramebuffers(renderPass);

    createBoxVertexBuffer();
    createBoxIndexBuffer();
    createGridVertexBuffer();
    createGridIndexBuffer();
    createUniformBuffers();
    createParticleShaderStorageBuffers();
    createStartEndParticleIdShaderStorageBuffers();
    createGraphicsDescriptorSets();
    createBoidsComputeDescriptorSets();
    createCellIdComputeDescriptorSets();
    createSyncObjects();
  }

  void initDearImgui()
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_instance->getVk();
    initInfo.PhysicalDevice = m_physicalDevice->getVk();
    initInfo.Device = m_device->GetVk();
    initInfo.QueueFamily = m_device->GetGraphicsAndComputeQueueFamily();
    initInfo.Queue = m_device->GetGraphicsQueue();
    initInfo.PipelineCache = nullptr;
    initInfo.DescriptorPoolSize = 2; // DearImgui will manage its own descriptor pool
    initInfo.RenderPass = renderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = m_swapChain->GetMinImageCount();
    initInfo.ImageCount = m_swapChain->GetImages().size();
    initInfo.MSAASamples = m_physicalDevice->GetMsaaSamples();
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&initInfo);
  }

  void createSurface()
  {
    if (glfwCreateWindowSurface(m_instance->getVk(), window, nullptr, &surface))
    {
      throw std::runtime_error("failed to create window surface!");
    }
  }

  void recreateSwapChain()
  {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    m_swapChain->recreate();
  }

  void createRenderPass()
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

    if (vkCreateRenderPass(m_device->GetVk(), &renderPassInfo, nullptr, &renderPass))
    {
      throw std::runtime_error("Failed to create render pass!");
    }
  }

  void createGraphicsDescriptorSetLayout()
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

  void createBoidsComputeDescriptorSetLayout()
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

  void createCellIdComputeDescriptorSetLayout()
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

  void createBoxGraphicsPipeline()
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
    pipelineInfo.renderPass = renderPass;
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

  void createGridGraphicsPipeline()
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
    pipelineInfo.renderPass = renderPass;
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

  void createParticleGraphicsPipeline()
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
    pipelineInfo.renderPass = renderPass;
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

  void createBoxVertexBuffer()
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

  void createBoxIndexBuffer()
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

  void createGridVertexBuffer()
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

  void createGridIndexBuffer()
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

  void createUniformBuffers()
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

  void createParticleShaderStorageBuffers()
  {
    particleShaderStorageBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    particleShaderStorageBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    // Initialize particles on CPU side before transferring them to GPU SSBOs
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    float boxLength = 2.0f;

    const auto& subdiv3D = Utils::GetNbParticlesSubdiv3D((Utils::NbParticles)PARTICLE_COUNT);
    glm::vec3 grid3DRes = { subdiv3D[0], subdiv3D[1], subdiv3D[2] };
    glm::vec3 start3D = { boxLength / -6.0f, boxLength / -6.0f, boxLength / -6.0f };
    glm::vec3 end3D = { boxLength / 6.0f, boxLength / 6.0f, boxLength / 6.0f };

    auto particlePositions = Geometry::Generate3DGrid(Geometry::Shape3D::Sphere, grid3DRes, start3D, end3D);

    // initial position is on a circle
    int i = 0;
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles)
    {
      float r = 0.25f * sqrt(rndDist(rndEngine));
      float theta = rndDist(rndEngine) * 2 * 3.14159;
      float x = r * cos(theta);
      float y = r * sin(theta);
      float z = r * (sin(theta) + cos(theta)) / 2.0;
      particle.position = particlePositions[i];
      particle.velocity = glm::normalize(particlePositions[i] * 0.00025f);
      particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
      particle.cellID = UINT32_MAX;
      i++;
    }

    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device->createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, particles.data(), (size_t)bufferSize);
    vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      m_device->createBuffer(bufferSize,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          particleShaderStorageBuffers[i], particleShaderStorageBuffersMemory[i]);
      // Copy from CPU to GPU
      m_device->copyBuffer(stagingBuffer, particleShaderStorageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
  }

  void createStartEndParticleIdShaderStorageBuffers()
  {
    startEndParticleIdShaderStorageBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    startEndParticleIdShaderStorageBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    std::array<glm::uvec2, GRID_SIZE> startEndParticleIds;
    std::fill_n(startEndParticleIds.begin(), GRID_SIZE, glm::uvec2(0, 0));

    VkDeviceSize bufferSize = sizeof(glm::uvec2) * GRID_SIZE;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    m_device->createBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_device->GetVk(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, startEndParticleIds.data(), (size_t)bufferSize);
    vkUnmapMemory(m_device->GetVk(), stagingBufferMemory);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      m_device->createBuffer(bufferSize,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          startEndParticleIdShaderStorageBuffers[i], startEndParticleIdShaderStorageBuffersMemory[i]);
      // Copy from CPU to GPU
      m_device->copyBuffer(stagingBuffer, startEndParticleIdShaderStorageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
  }

  void createGraphicsDescriptorSets()
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
      storageBufferInfoStartEndParticleId.buffer = startEndParticleIdShaderStorageBuffers[i];
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

  void createBoidsComputeDescriptorSets()
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
      storageBufferInfoLastFrame.buffer = particleShaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT];
      storageBufferInfoLastFrame.offset = 0;
      storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

      VkDescriptorBufferInfo storageBufferInfoCurrentFrame {};
      storageBufferInfoCurrentFrame.buffer = particleShaderStorageBuffers[i];
      storageBufferInfoCurrentFrame.offset = 0;
      storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

      VkDescriptorBufferInfo storageBufferInfoStartEndParticleId {};
      storageBufferInfoStartEndParticleId.buffer = startEndParticleIdShaderStorageBuffers[i];
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

  void createCellIdComputeDescriptorSets()
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
      storageBufferInfoParticle.buffer = particleShaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT];
      storageBufferInfoParticle.offset = 0;
      storageBufferInfoParticle.range = sizeof(Particle) * PARTICLE_COUNT;

      VkDescriptorBufferInfo storageBufferInfoStartEndParticleId {};
      storageBufferInfoStartEndParticleId.buffer = startEndParticleIdShaderStorageBuffers[i];
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

  void createSyncObjects()
  {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      if (vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
          || vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
          || vkCreateSemaphore(m_device->GetVk(), &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS
          || vkCreateFence(m_device->GetVk(), &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS
          || vkCreateFence(m_device->GetVk(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
      {
        throw std::runtime_error("Failed to create semaphores!");
      }
    }
  }

  void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
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
    renderPassInfo.renderPass = renderPass;
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

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &particleShaderStorageBuffers[currentFrame], offsets);

    vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);

    // UI rendering
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to record the graphics command buffer!");
    }
  }

  void recordComputeCommandBuffer(VkCommandBuffer commandBuffer)
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

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to record the compute command buffer!");
    }
  }

  void mainLoop()
  {
    while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();

      simulateParticles();

      renderUI();

      drawFrame();
    }

    vkDeviceWaitIdle(m_device->GetVk());
  }

  void simulateParticles()
  {
    // Compute Submission
    vkWaitForFences(m_device->GetVk(), 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    updateComputeUniformBuffer(currentFrame);

    vkResetFences(m_device->GetVk(), 1, &computeInFlightFences[currentFrame]);

    auto computeCommandBuffer = m_device->getComputeCommandBuffer(currentFrame);

    vkResetCommandBuffer(computeCommandBuffer, 0);

    recordComputeCommandBuffer(computeCommandBuffer);

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

  void renderUI()
  {
    // Start the Dear ImGui frame and render UI items in DearImGui
    // Note that real drawing is embedded with ours and will happen later
    // Indeed, we add UI draw calls to our command buffer in recordGraphicsCommandBuffer()
    // and this command buffer is submitted in drawFrame()
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    {
      ImGui::Begin("Welcome to VkClouds!");
      ImGui::Text("This is a small application made for learning purpose.");
      ImGui::Text("It follows the awesome Vulkan tutorial on https://docs.vulkan.org.");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }

    ImGui::Render();
  }

  void drawFrame()
  {
    // wait for the previous frame to be finished
    vkWaitForFences(m_device->GetVk(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_device->GetVk(), m_swapChain->GetVk(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
      framebufferResized = false;
      recreateSwapChain();
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

    recordGraphicsCommandBuffer(graphicsCommandBuffer, imageIndex);

    VkSubmitInfo graphicsSubmitInfo {};
    graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame] };
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

  void updateUniformBuffer(uint32_t currentImage)
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

  void updateComputeUniformBuffer(uint32_t currentImage)
  {
    ComputeUniformBufferObject ubo {};
    ubo.deltaTime = 0.001f;

    memcpy(computeUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  void cleanup()
  {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_swapChain->cleanup();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      vkDestroyBuffer(m_device->GetVk(), uniformBuffers[i], nullptr);
      vkFreeMemory(m_device->GetVk(), uniformBuffersMemory[i], nullptr);

      vkDestroyBuffer(m_device->GetVk(), particleShaderStorageBuffers[i], nullptr);
      vkFreeMemory(m_device->GetVk(), particleShaderStorageBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(m_device->GetVk(), graphicsDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device->GetVk(), boidsComputeDescriptorSetLayout, nullptr);

    vkDestroyBuffer(m_device->GetVk(), boxVertexBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), boxVertexBufferMemory, nullptr);

    vkDestroyBuffer(m_device->GetVk(), boxIndexBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), boxIndexBufferMemory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      vkDestroySemaphore(m_device->GetVk(), imageAvailableSemaphores[i], nullptr);
      vkDestroySemaphore(m_device->GetVk(), renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(m_device->GetVk(), computeFinishedSemaphores[i], nullptr);
      vkDestroyFence(m_device->GetVk(), inFlightFences[i], nullptr);
      vkDestroyFence(m_device->GetVk(), computeInFlightFences[i], nullptr);
    }

    vkDestroyPipeline(m_device->GetVk(), boxGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device->GetVk(), boxGraphicsPipelineLayout, nullptr);

    vkDestroyPipeline(m_device->GetVk(), particleGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device->GetVk(), particleGraphicsPipelineLayout, nullptr);

    vkDestroyPipeline(m_device->GetVk(), boidsCompPipeline.first, nullptr);
    vkDestroyPipelineLayout(m_device->GetVk(), boidsCompPipeline.second, nullptr);

    vkDestroyRenderPass(m_device->GetVk(), renderPass, nullptr);

    vkDestroySurfaceKHR(m_instance->getVk(), surface, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
  }
};

int main()
{
  Utils::InitializeLogger();

  LOG_DEBUG("Launch VkClouds app");

  try
  {
    auto app = VkCloudsApp();

    app.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
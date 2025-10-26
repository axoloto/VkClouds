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

#include "GraphicsEngine.hpp"
#include "PhysicsEngine.hpp"
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
  std::unique_ptr<GraphicsEngine> m_graphicsEngine;
  std::unique_ptr<PhysicsEngine> m_physicsEngine;

  VkSurfaceKHR surface;

  uint32_t currentFrame = 0;

  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> particleSSBOs;
  std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> particleSSBOsMemory;
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> startEndParticleIdSSBOs;
  std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> startEndParticleIdSSBOsMemory;

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

    createParticleSSBOs();
    createStartEndParticleIdSSBOs();

    std::function<void(VkCommandBuffer)> drawUIFunc = [](VkCommandBuffer commandBuffer)
    { ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer); };

    m_graphicsEngine = std::make_unique<GraphicsEngine>(m_swapChain.get(),
        m_physicalDevice.get(), m_device.get(),
        particleSSBOs,
        startEndParticleIdSSBOs,
        drawUIFunc);

    m_physicsEngine = std::make_unique<PhysicsEngine>(m_device.get(),
        particleSSBOs,
        startEndParticleIdSSBOs);

    m_swapChain->createFramebuffers(m_graphicsEngine->renderPass());
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
    initInfo.RenderPass = m_graphicsEngine->renderPass();
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

  void createParticleSSBOs()
  {
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
          particleSSBOs[i], particleSSBOsMemory[i]);
      // Copy from CPU to GPU
      m_device->copyBuffer(stagingBuffer, particleSSBOs[i], bufferSize);
    }

    vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
  }

  void createStartEndParticleIdSSBOs()
  {
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
          startEndParticleIdSSBOs[i], startEndParticleIdSSBOsMemory[i]);
      // Copy from CPU to GPU
      m_device->copyBuffer(stagingBuffer, startEndParticleIdSSBOs[i], bufferSize);
    }

    vkDestroyBuffer(m_device->GetVk(), stagingBuffer, nullptr);
    vkFreeMemory(m_device->GetVk(), stagingBufferMemory, nullptr);
  }

  void mainLoop()
  {
    while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();

      m_physicsEngine->simulate(currentFrame);

      renderUI();

      m_graphicsEngine->draw(currentFrame, m_physicsEngine->computeFinishedSemaphore(currentFrame));
    }

    vkDeviceWaitIdle(m_device->GetVk());
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

  void cleanup()
  {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_swapChain->cleanup();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      vkDestroyBuffer(m_device->GetVk(), particleSSBOs[i], nullptr);
      vkFreeMemory(m_device->GetVk(), particleSSBOsMemory[i], nullptr);
    }

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
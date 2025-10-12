#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

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
  PhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
  ~PhysicalDevice();
  VkPhysicalDevice GetVkPhysicalDevice() { return m_physicalDevice; }
  QueueFamilyIndices GetQueueFamilyIndices() { return m_queueFamilyIndices; }
  VkSampleCountFlagBits GetMsaaSamples() { return m_msaaSamples; }
  SwapChainSupportDetails GetSwapChainSupportDetails() { return m_swapChainSupportDetails; }

  private:
  VkPhysicalDevice m_physicalDevice;
  QueueFamilyIndices m_queueFamilyIndices;
  VkSampleCountFlagBits m_msaaSamples;
  SwapChainSupportDetails m_swapChainSupportDetails;
};
}
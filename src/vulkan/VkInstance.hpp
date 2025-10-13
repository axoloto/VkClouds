#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace vk
{
class Instance
{
  public:
  Instance(const std::string& appName, std::vector<const char*> requiredExtensionNames);
  ~Instance();
  VkInstance getVk() { return m_instance; }

  private:
  void setupDebugMessenger();

  VkInstance m_instance;
  VkDebugUtilsMessengerEXT m_debugMessenger;
};
}
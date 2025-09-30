#include "Utils.hpp"
#include <fstream>
#include <iostream>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

std::vector<char> readShaderFile(const std::string& shaderName)
{
  return readFile(std::string(TOSTRING(SHADER_DIR)) + shaderName);
}

std::vector<char> readFile(const std::string& fileName)
{
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);

  if (!file.is_open())
  {
    throw std::runtime_error("Failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

std::string getTexturePath(const std::string& textureName)
{
  return std::string(TOSTRING(TEXTURE_DIR)) + textureName;
}

std::string getModelPath(const std::string& modelName)
{
  return std::string(TOSTRING(MODEL_DIR)) + modelName;
}

#pragma once

#include <string>
#include <vector>

std::vector<char> readShaderFile(const std::string& shaderName);
std::vector<char> readFile(const std::string& fileName);
std::string getTexturePath(const std::string& textureName);
std::string getModelPath(const std::string& modelName);

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t PARTICLE_COUNT = 512;
static constexpr uint32_t GRID_RES = 8;
static constexpr uint32_t GRID_SIZE = GRID_RES * GRID_RES * GRID_RES;

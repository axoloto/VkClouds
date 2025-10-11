#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace Geometry
{
enum class Dimension
{
  dim2D,
  dim3D
};

enum class Shape2D
{
  Rectangle,
  Circle
};

enum class Shape3D
{
  Box,
  Sphere
};

enum class Distribution
{
  Uniform,
  Random
};

enum class Plane
{
  XY,
  YZ,
  XZ
};

struct BoxSize3D
{
  size_t x;
  size_t y;
  size_t z;
};

// Dimensions of the bounding box where the particles evolve
static constexpr BoxSize3D BOX_SIZE_3D = { 10, 10, 10 };

// Resolution of the cells forming the 3D grid containing all the particles
static constexpr BoxSize3D GRID_RES_3D = { 30, 30, 30 };

// 3D Box
using Vertex3D = std::array<float, 3>;
static constexpr std::array<Vertex3D, 8> RefCubeVertices {
  Vertex3D({ 1.f, -1.f, -1.f }),
  Vertex3D({ 1.f, 1.f, -1.f }),
  Vertex3D({ -1.f, 1.f, -1.f }),
  Vertex3D({ -1.f, -1.f, -1.f }),
  Vertex3D({ 1.f, -1.f, 1.f }),
  Vertex3D({ 1.f, 1.f, 1.f }),
  Vertex3D({ -1.f, 1.f, 1.f }),
  Vertex3D({ -1.f, -1.f, 1.f })
};

static constexpr std::array<std::uint32_t, 24> RefCubeIndices {
  0, 1,
  1, 2,
  2, 3,
  3, 0,
  4, 5,
  5, 6,
  6, 7,
  7, 4,
  0, 4,
  1, 5,
  2, 6,
  3, 7
};

// 2D Box
using Vertex2D = std::array<float, 2>;
static constexpr std::array<Vertex2D, 4> RefSquareVertices {
  Vertex2D({ -1.f, -1.f }),
  Vertex2D({ -1.f, 1.f }),
  Vertex2D({ 1.f, 1.f }),
  Vertex2D({ 1.f, -1.f }),
};

static constexpr std::array<std::uint32_t, 24> RefSquareIndices {
  0, 1,
  1, 2,
  2, 3,
  3, 0
};

std::vector<glm::vec3> Generate2DGrid(Shape2D shape, Plane plane,
    glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution = Distribution::Uniform);

std::vector<glm::vec3> Generate3DGrid(Shape3D shape,
    glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution = Distribution::Uniform);

void GenerateRectangularGrid(Plane plane, std::vector<glm::vec3>& verts, glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution = Distribution::Uniform);
void GenerateCircularGrid(Plane plane, std::vector<glm::vec3>& verts, glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos);
void GenerateBoxGrid(std::vector<glm::vec3>& verts, glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution = Distribution::Uniform);
void GenerateSphereGrid(std::vector<glm::vec3>& verts, glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos);
}
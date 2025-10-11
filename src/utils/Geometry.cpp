
#include "Geometry.hpp"
#include "Logging.hpp"
#include <cmath>

namespace Geometry
{
std::vector<glm::vec3> Generate2DGrid(Shape2D shape, Plane plane,
    glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution)
{
  if (distribution != Distribution::Uniform && shape != Shape2D::Rectangle)
  {
    LOG_ERROR("Random distribution not implemented yet for this shape");
    return std::vector<glm::vec3>();
  }

  if (gridRes.x * gridRes.y <= 0)
  {
    LOG_ERROR("Cannot generate grid with negative or null number of vertices");
    return std::vector<glm::vec3>();
  }

  std::vector<glm::vec3> verts(gridRes.x * gridRes.y, glm::vec3(0.0f, 0.0f, 0.0f));

  switch (shape)
  {
  case Shape2D::Circle:
    GenerateCircularGrid(plane, verts, gridRes, gridStartPos, gridEndPos);
    break;
  case Shape2D::Rectangle:
    GenerateRectangularGrid(plane, verts, gridRes, gridStartPos, gridEndPos, distribution);
    break;
  default:
    break;
  }

  return verts;
}

std::vector<glm::vec3> Generate3DGrid(Shape3D shape,
    glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution)
{
  if (distribution != Distribution::Uniform && shape != Shape3D::Box)
  {
    LOG_ERROR("Random distribution not implemented yet for this shape");
    return std::vector<glm::vec3>();
  }

  if (gridRes.x * gridRes.y * gridRes.z <= 0)
  {
    LOG_ERROR("Cannot generate grid with negative or null number of vertices");
    return std::vector<glm::vec3>();
  }

  std::vector<glm::vec3> verts(gridRes.x * gridRes.y * gridRes.z, glm::vec3(0.0f, 0.0f, 0.0f));

  switch (shape)
  {
  case Shape3D::Sphere:
    GenerateSphereGrid(verts, gridRes, gridStartPos, gridEndPos);
    break;
  case Shape3D::Box:
    GenerateBoxGrid(verts, gridRes, gridStartPos, gridEndPos, distribution);
  default:
    break;
  }

  return verts;
}

void GenerateRectangularGrid(Plane plane, std::vector<glm::vec3>& verts, glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution)
{
  int nbVertices = gridRes.x * gridRes.y;

  if (verts.size() < nbVertices)
  {
    LOG_ERROR("Cannot generate grid with this resolution");
    return;
  }

  glm::vec3 vec = gridEndPos - gridStartPos;

  glm::vec3 gridSpacing;
  glm::vec3 gridResExt;

  switch (plane)
  {
  case Plane::XY:
    gridSpacing = glm::vec3({ vec.x / gridRes.x, vec.y / gridRes.y, 0.0f });
    gridResExt = glm::vec3({ gridRes.x, gridRes.y, 1 });
    break;
  case Plane::XZ:
    gridSpacing = glm::vec3({ vec.x / gridRes.x, 0.0f, vec.z / gridRes.y });
    gridResExt = glm::vec3({ gridRes.x, 1, gridRes.y });
    break;
  case Plane::YZ:
    gridSpacing = glm::vec3({ 0.0f, vec.y / gridRes.x, vec.z / gridRes.y });
    gridResExt = glm::vec3({ 1, gridRes.x, gridRes.y });
    break;
  default:
    LOG_ERROR("Cannot generate rectangular grid. Plane not existing");
    break;
  }

  int vertIndex = 0;
  if (distribution == Distribution::Uniform)
  {
    for (int ix = 0; ix < gridResExt.x; ++ix)
    {
      for (int iy = 0; iy < gridResExt.y; ++iy)
      {
        for (int iz = 0; iz < gridResExt.z; ++iz)
        {
          verts[vertIndex++] = {
            gridStartPos.x + ix * gridSpacing.x,
            gridStartPos.y + iy * gridSpacing.y,
            gridStartPos.z + iz * gridSpacing.z
          };
        }
      }
    }
  }
  else if (distribution == Distribution::Random)
  {
    for (int i = 0; i < nbVertices; ++i)
    {
      verts[vertIndex++] = {
        (float)rand() / (float)RAND_MAX * vec.x + gridStartPos.x,
        (float)rand() / (float)RAND_MAX * vec.y + gridStartPos.y,
        (float)rand() / (float)RAND_MAX * vec.z + gridStartPos.z
      };
    }
  }
}

void GenerateCircularGrid(Plane plane, std::vector<glm::vec3>& verts, glm::vec2 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos)
{
  if (verts.size() < gridRes.x * gridRes.y)
  {
    LOG_ERROR("Cannot generate grid with this resolution");
    return;
  }

  glm::vec3 vec = gridEndPos - gridStartPos;
  glm::vec3 gridCenterPos = gridStartPos + vec / 2.0f;
  float radius = glm::length(vec) / 2.0f;
  float angleSpacing = 2.0f * glm::pi<float>() / gridRes.x;
  float radiusSpacing = radius / gridRes.y;
  int vertIndex = 0;

  switch (plane)
  {
  case Plane::XY:
    for (int io = 0; io < gridRes.x; ++io)
    {
      for (int ir = 1; ir < gridRes.y + 1; ++ir)
      {
        verts[vertIndex++] = {
          gridCenterPos.x + (ir * radiusSpacing) * std::cos(io * angleSpacing),
          gridCenterPos.y + (ir * radiusSpacing) * std::sin(io * angleSpacing),
          gridCenterPos.z
        };
      }
    }
    break;
  case Plane::XZ:
    for (int io = 0; io < gridRes.x; ++io)
    {
      for (int ir = 1; ir < gridRes.y + 1; ++ir)
      {
        verts[vertIndex++] = {
          gridCenterPos.x + (ir * radiusSpacing) * std::cos(io * angleSpacing),
          gridCenterPos.y,
          gridCenterPos.z + (ir * radiusSpacing) * std::sin(io * angleSpacing)
        };
      }
    }
    break;
  case Plane::YZ:
    for (int io = 0; io < gridRes.x; ++io)
    {
      for (int ir = 0; ir < gridRes.y; ++ir)
      {
        verts[vertIndex++] = {
          gridCenterPos.x,
          gridCenterPos.y + ((ir + 1) * radiusSpacing) * std::cos(io * angleSpacing),
          gridCenterPos.z + ((ir + 1) * radiusSpacing) * std::sin(io * angleSpacing)
        };
      }
    }
    break;
  default:
    LOG_ERROR("Cannot generate rectangular grid. Plane not existing");
    break;
  }
}

void GenerateBoxGrid(std::vector<glm::vec3>& verts, glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos, Distribution distribution)
{
  int nbVertices = gridRes.x * gridRes.y * gridRes.z;

  if (verts.size() < nbVertices)
  {
    LOG_ERROR("Cannot generate grid with this resolution");
    return;
  }

  glm::vec3 vec = gridEndPos - gridStartPos;
  glm::vec3 gridSpacing = glm::vec3({ vec.x / gridRes.x, vec.y / gridRes.y, vec.z / gridRes.z });

  int vertIndex = 0;
  if (distribution == Distribution::Uniform)
  {
    for (int ix = 0; ix < gridRes.x; ++ix)
    {
      for (int iy = 0; iy < gridRes.y; ++iy)
      {
        for (int iz = 0; iz < gridRes.z; ++iz)
        {
          verts[vertIndex++] = {
            gridStartPos.x + ix * gridSpacing.x,
            gridStartPos.y + iy * gridSpacing.y,
            gridStartPos.z + iz * gridSpacing.z
          };
        }
      }
    }
  }
  else if (distribution == Distribution::Random)
  {
    for (int i = 0; i < nbVertices; ++i)
    {
      verts[vertIndex++] = {
        (float)rand() / (float)RAND_MAX * vec.x + gridStartPos.x,
        (float)rand() / (float)RAND_MAX * vec.y + gridStartPos.y,
        (float)rand() / (float)RAND_MAX * vec.z + gridStartPos.z
      };
    }
  }
}

void GenerateSphereGrid(std::vector<glm::vec3>& verts, glm::vec3 gridRes, glm::vec3 gridStartPos, glm::vec3 gridEndPos)
{
  if (verts.size() < gridRes.x * gridRes.y * gridRes.z)
  {
    LOG_ERROR("Cannot generate grid with this resolution");
    return;
  }

  glm::vec3 vec = gridEndPos - gridStartPos;
  glm::vec3 gridCenterPos = gridStartPos + vec / 2.0f;
  float radius = glm::length(vec) / 2.0f;
  float phiSpacing = glm::pi<float>() / gridRes.x;
  float thetaSpacing = 2.0f * glm::pi<float>() / gridRes.y;
  float radiusSpacing = radius / gridRes.z;

  int vertIndex = 0;
  for (int iphi = 0; iphi < gridRes.x; ++iphi)
  {
    for (int itheta = 0; itheta < gridRes.y; ++itheta)
    {
      for (int ir = 0; ir < gridRes.z; ++ir)
      {
        verts[vertIndex++] = {
          gridCenterPos.x + ((ir + 1) * radiusSpacing) * std::cos(itheta * thetaSpacing) * std::sin(iphi * phiSpacing),
          gridCenterPos.y + ((ir + 1) * radiusSpacing) * std::sin(itheta * thetaSpacing) * std::sin(iphi * phiSpacing),
          gridCenterPos.z + ((ir + 1) * radiusSpacing) * std::cos(iphi * phiSpacing),
        };
      }
    }
  }
}
}
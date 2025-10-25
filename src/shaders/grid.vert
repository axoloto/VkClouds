#version 450

#include "define.glsl"
#include "grid_utils.glsl"

layout(binding = 0) uniform UniformBufferObject
{
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

layout(std430, binding = 1) readonly buffer StartEndPartIdSSBO
{
  uvec2 cStartEndPartIds[];
};

layout(location = 0) in vec3 inPosition; // from binding 0
layout(location = 1) in vec3 inColor; // from binding 0
layout(location = 2) in vec2 inTexCoord; // from binding 0

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
  uint cellId = getCell1DIndexFromPos(inPosition);

  uvec2 startEndCellId = cStartEndPartIds[511];

  // grid cell is active
  if (startEndCellId.y > startEndCellId.x)
  {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
  }
  else
  {
    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
  }

  fragColor = inColor;
  fragTexCoord = inTexCoord;
}
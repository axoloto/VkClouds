#version 450

layout(binding = 0) uniform UniformBufferObject
{
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

layout(location = 0) in vec4 inPosition; // from binding 0
layout(location = 1) in vec4 inColor; // from binding 0

layout(location = 0) out vec3 fragColor;

void main()
{
  gl_Position = ubo.proj * ubo.view * ubo.model * inPosition;
  gl_PointSize = 4.0;
  fragColor = inColor.rgb;
}
file(GLOB SRC "VkCloudsApp.cpp")

add_executable(VkCloudsApp ${SRC})
set_target_properties(VkCloudsApp PROPERTIES FOLDER app)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(VkCloudsApp PUBLIC DEBUG_BUILD)
endif()

find_package(glfw3 REQUIRED)
find_package(VulkanLoader REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(stb REQUIRED)
find_package(tinyobjloader REQUIRED)

add_subdirectory(imgui_backend)

target_link_libraries(VkCloudsApp PRIVATE utils imgui_backend Vulkan::Loader vulkan-headers::vulkan-headers glfw stb::stb tinyobjloader::tinyobjloader)

install(TARGETS VkCloudsApp RUNTIME DESTINATION bin)
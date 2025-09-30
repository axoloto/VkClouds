file(GLOB SRC "VkToyApp.cpp" "Utils.cpp" "*.hpp")

add_executable(VkToyApp ${SRC})
set_target_properties(VkToyApp PROPERTIES FOLDER app)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(VkToyApp PUBLIC DEBUG_BUILD)
endif()

target_compile_definitions(VkToyApp PRIVATE SHADER_DIR=${SHADER_DIR})
target_compile_definitions(VkToyApp PRIVATE TEXTURE_DIR=${TEXTURE_DIR})
target_compile_definitions(VkToyApp PRIVATE MODEL_DIR=${MODEL_DIR})

find_package(glfw3 REQUIRED)
find_package(VulkanLoader REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(tinyobjloader REQUIRED)

add_subdirectory(imgui_backend)

target_link_libraries(VkToyApp PRIVATE imgui_backend Vulkan::Loader vulkan-headers::vulkan-headers glfw spdlog::spdlog glm::glm stb::stb tinyobjloader::tinyobjloader)

install(TARGETS VkToyApp RUNTIME DESTINATION bin)
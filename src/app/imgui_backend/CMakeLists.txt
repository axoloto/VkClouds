file(GLOB SRC "*.cpp" "*.h")

#add_compile_definitions(IMGUI_IMPL_VULKAN_LOADER_GLAD)

find_package(imgui REQUIRED CONFIG)
find_package(glfw3 REQUIRED CONFIG)
find_package(VulkanLoader REQUIRED CONFIG)
find_package(VulkanHeaders REQUIRED CONFIG)

add_library(imgui_backend ${SRC})

target_include_directories(imgui_backend INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(imgui_backend PUBLIC imgui::imgui glfw Vulkan::Loader vulkan-headers::vulkan-headers)

install(TARGETS imgui_backend DESTINATION lib)
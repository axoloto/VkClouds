file(GLOB SRC "*.cpp" "*.hpp")

find_package(VulkanLoader REQUIRED)
find_package(VulkanHeaders REQUIRED)

add_library(vulkan ${SRC})

target_link_libraries(vulkan PUBLIC utils Vulkan::Loader vulkan-headers::vulkan-headers)

target_include_directories(vulkan INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS vulkan DESTINATION lib)
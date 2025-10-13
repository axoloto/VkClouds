file(GLOB SRC "*.cpp" "*.hpp")

add_executable(VkCloudsApp ${SRC})
set_target_properties(VkCloudsApp PROPERTIES FOLDER app)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(VkCloudsApp PUBLIC DEBUG_BUILD)
endif()

find_package(stb REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(imgui_backend)

target_link_libraries(VkCloudsApp PRIVATE vulkan utils imgui_backend glfw stb::stb tinyobjloader::tinyobjloader)

install(TARGETS VkCloudsApp RUNTIME DESTINATION bin)
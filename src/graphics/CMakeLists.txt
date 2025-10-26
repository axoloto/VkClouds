file(GLOB SRC "*.cpp" "*.hpp")

add_library(graphics ${SRC})

target_link_libraries(graphics PRIVATE utils vulkan)

target_include_directories(graphics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS graphics DESTINATION lib)
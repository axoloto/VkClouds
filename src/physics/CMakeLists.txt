file(GLOB SRC "*.cpp" "*.hpp")

add_library(physics ${SRC})

target_link_libraries(physics PRIVATE utils vulkan)

target_include_directories(physics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS physics DESTINATION lib)
file(GLOB SRC "*.cpp" "*.hpp")

find_package(glm REQUIRED)
find_package(spdlog REQUIRED)

add_library(utils ${SRC})

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(utils PUBLIC DEBUG_BUILD)
endif()

target_compile_definitions(utils PRIVATE SHADER_DIR=${SHADER_DIR})
target_compile_definitions(utils PRIVATE TEXTURE_DIR=${TEXTURE_DIR})
target_compile_definitions(utils PRIVATE MODEL_DIR=${MODEL_DIR})
message(STATUS "Shader Directory: ${SHADER_DIR}")
message(STATUS "Texture Directory: ${TEXTURE_DIR}")
message(STATUS "Model Directory: ${MODEL_DIR}")

target_compile_definitions(utils PUBLIC -DSOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_definitions(utils PUBLIC -DINSTALL_DIR="${CMAKE_INSTALL_DIR}")
target_compile_definitions(utils PUBLIC -DVERSION_MAJOR="${PROJECT_VERSION_MAJOR}")
target_compile_definitions(utils PUBLIC -DVERSION_MINOR="${PROJECT_VERSION_MINOR}")
target_compile_definitions(utils PUBLIC -DVERSION_PATCH="${PROJECT_VERSION_PATCH}")
message(STATUS "CMAKE Source Directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE Install Directory: ${CMAKE_INSTALL_DIR}")

target_link_libraries(utils PUBLIC glm::glm spdlog::spdlog)

target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS utils DESTINATION lib)
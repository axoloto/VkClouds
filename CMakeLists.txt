cmake_minimum_required(VERSION 3.28)

project(VkToy VERSION 0.0.1
              DESCRIPTION ""
              HOMEPAGE_URL "https://github.com/axoloto/VkToy"
              LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 3rd party deps
#include(cmake/Conan.cmake)
#run_conan()

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/ProjectOptions.cmake)

VkToy_setup_options()
VkToy_global_options()
VkToy_local_options()

# don't know if this should be set globally from here or not...
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)

target_compile_features(VkToy_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(VkToy::VkToy_options ALIAS VkToy_options)
add_library(VkToy::VkToy_warnings ALIAS VkToy_warnings)

#add_library(VkToy::VkToy_options INTERFACE IMPORTED)
#add_library(VkToy::VkToy_warnings INTERFACE IMPORTED)

# configure files based on CMake configuration options
add_subdirectory(configured_files)

# Adding the src:
add_subdirectory(src)

# Don't even look at tests if we're not top level
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

set(ENABLE_TESTING OFF)
if(ENABLE_TESTING)
  # Adding the tests:
  include(CTest)

  if(BUILD_TESTING)
    message(AUTHOR_WARNING "Building Tests. Be sure to check out test/constexpr_tests.cpp for constexpr testing")
    add_subdirectory(test)
  endif()

  if(VkToy_BUILD_FUZZ_TESTS)
    message(AUTHOR_WARNING "Building Fuzz Tests, using fuzzing sanitizer https://www.llvm.org/docs/LibFuzzer.html")
    if (NOT VkToy_ENABLE_ADDRESS_SANITIZER AND NOT VkToy_ENABLE_THREAD_SANITIZER)
      message(WARNING "You need asan or tsan enabled for meaningful fuzz testing")
    endif()
    add_subdirectory(fuzz_test)
  endif()
endif()

# If MSVC is being used, and ASAN is enabled, we need to set the debugger environment
# so that it behaves well with MSVC's debugger, and we can run the target from visual studio
if(MSVC)
  get_all_installable_targets(all_targets)
  message("all_targets=${all_targets}")
  set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$(VC_ExecutablePath_x64);%PATH%")
endif()

# set the startup project for the "play" button in MSVC
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT VkToyApp)

set(ENABLE_PACKAGING OFF)
if(ENABLE_PACKAGING)
  include(cmake/PackageProject.cmake)

  # Add other targets that you want installed here, by default we just package the one executable
  # we know we want to ship
  VkToy_package_project(
    TARGETS
    VkToySpp
    VkToy_options
    VkToy_warnings
    # FIXME: this does not work! CK
    # PRIVATE_DEPENDENCIES_CONFIGURED project_options project_warnings
  )

  # Experience shows that explicit package naming can help make it easier to sort
  # out potential ABI related issues before they start, while helping you
  # track a build to a specific GIT SHA
  set(CPACK_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
  )

  include(CPack)
endif()

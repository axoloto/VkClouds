include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(VkClouds_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
    set(SUPPORTS_UBSAN ON)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    set(SUPPORTS_ASAN ON)
  endif()
endmacro()

macro(VkClouds_setup_options)
  option(VkClouds_ENABLE_HARDENING "Enable hardening" ON)
  option(VkClouds_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    VkClouds_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    VkClouds_ENABLE_HARDENING
    OFF)

  VkClouds_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR VkClouds_PACKAGING_MAINTAINER_MODE)
    option(VkClouds_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(VkClouds_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(VkClouds_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(VkClouds_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(VkClouds_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(VkClouds_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(VkClouds_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(VkClouds_ENABLE_PCH "Enable precompiled headers" OFF)
    option(VkClouds_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(VkClouds_ENABLE_IPO "Enable IPO/LTO" ON)
    option(VkClouds_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(VkClouds_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(VkClouds_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(VkClouds_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(VkClouds_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(VkClouds_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(VkClouds_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(VkClouds_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(VkClouds_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(VkClouds_ENABLE_PCH "Enable precompiled headers" OFF)
    option(VkClouds_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      VkClouds_ENABLE_IPO
      VkClouds_WARNINGS_AS_ERRORS
      VkClouds_ENABLE_USER_LINKER
      VkClouds_ENABLE_SANITIZER_ADDRESS
      VkClouds_ENABLE_SANITIZER_LEAK
      VkClouds_ENABLE_SANITIZER_UNDEFINED
      VkClouds_ENABLE_SANITIZER_THREAD
      VkClouds_ENABLE_SANITIZER_MEMORY
      VkClouds_ENABLE_UNITY_BUILD
      VkClouds_ENABLE_CLANG_TIDY
      VkClouds_ENABLE_CPPCHECK
      VkClouds_ENABLE_COVERAGE
      VkClouds_ENABLE_PCH
      VkClouds_ENABLE_CACHE)
  endif()

  VkClouds_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (VkClouds_ENABLE_SANITIZER_ADDRESS OR VkClouds_ENABLE_SANITIZER_THREAD OR VkClouds_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(VkClouds_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(VkClouds_global_options)
  if(VkClouds_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    VkClouds_enable_ipo()
  endif()

  VkClouds_supports_sanitizers()

  if(VkClouds_ENABLE_HARDENING AND VkClouds_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR VkClouds_ENABLE_SANITIZER_UNDEFINED
       OR VkClouds_ENABLE_SANITIZER_ADDRESS
       OR VkClouds_ENABLE_SANITIZER_THREAD
       OR VkClouds_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${VkClouds_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${VkClouds_ENABLE_SANITIZER_UNDEFINED}")
    VkClouds_enable_hardening(VkClouds_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(VkClouds_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(VkClouds_warnings INTERFACE)
  add_library(VkClouds_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  VkClouds_set_project_warnings(
    VkClouds_warnings
    ${VkClouds_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(VkClouds_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    configure_linker(VkClouds_options)
  endif()

  include(cmake/Sanitizers.cmake)
  VkClouds_enable_sanitizers(
    VkClouds_options
    ${VkClouds_ENABLE_SANITIZER_ADDRESS}
    ${VkClouds_ENABLE_SANITIZER_LEAK}
    ${VkClouds_ENABLE_SANITIZER_UNDEFINED}
    ${VkClouds_ENABLE_SANITIZER_THREAD}
    ${VkClouds_ENABLE_SANITIZER_MEMORY})

  set_target_properties(VkClouds_options PROPERTIES UNITY_BUILD ${VkClouds_ENABLE_UNITY_BUILD})

  if(VkClouds_ENABLE_PCH)
    target_precompile_headers(
      VkClouds_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(VkClouds_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    VkClouds_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(VkClouds_ENABLE_CLANG_TIDY)
    VkClouds_enable_clang_tidy(VkClouds_options ${VkClouds_WARNINGS_AS_ERRORS})
  endif()

  if(VkClouds_ENABLE_CPPCHECK)
    VkClouds_enable_cppcheck(${VkClouds_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(VkClouds_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    VkClouds_enable_coverage(VkClouds_options)
  endif()

  if(VkClouds_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(VkClouds_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(VkClouds_ENABLE_HARDENING AND NOT VkClouds_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR VkClouds_ENABLE_SANITIZER_UNDEFINED
       OR VkClouds_ENABLE_SANITIZER_ADDRESS
       OR VkClouds_ENABLE_SANITIZER_THREAD
       OR VkClouds_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    VkClouds_enable_hardening(VkClouds_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()

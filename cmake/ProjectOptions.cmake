include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(VkToy_supports_sanitizers)
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

macro(VkToy_setup_options)
  option(VkToy_ENABLE_HARDENING "Enable hardening" ON)
  option(VkToy_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    VkToy_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    VkToy_ENABLE_HARDENING
    OFF)

  VkToy_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR VkToy_PACKAGING_MAINTAINER_MODE)
    option(VkToy_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(VkToy_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(VkToy_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(VkToy_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(VkToy_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(VkToy_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(VkToy_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(VkToy_ENABLE_PCH "Enable precompiled headers" OFF)
    option(VkToy_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(VkToy_ENABLE_IPO "Enable IPO/LTO" ON)
    option(VkToy_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(VkToy_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(VkToy_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(VkToy_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(VkToy_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(VkToy_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(VkToy_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(VkToy_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(VkToy_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(VkToy_ENABLE_PCH "Enable precompiled headers" OFF)
    option(VkToy_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      VkToy_ENABLE_IPO
      VkToy_WARNINGS_AS_ERRORS
      VkToy_ENABLE_USER_LINKER
      VkToy_ENABLE_SANITIZER_ADDRESS
      VkToy_ENABLE_SANITIZER_LEAK
      VkToy_ENABLE_SANITIZER_UNDEFINED
      VkToy_ENABLE_SANITIZER_THREAD
      VkToy_ENABLE_SANITIZER_MEMORY
      VkToy_ENABLE_UNITY_BUILD
      VkToy_ENABLE_CLANG_TIDY
      VkToy_ENABLE_CPPCHECK
      VkToy_ENABLE_COVERAGE
      VkToy_ENABLE_PCH
      VkToy_ENABLE_CACHE)
  endif()

  VkToy_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (VkToy_ENABLE_SANITIZER_ADDRESS OR VkToy_ENABLE_SANITIZER_THREAD OR VkToy_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(VkToy_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(VkToy_global_options)
  if(VkToy_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    VkToy_enable_ipo()
  endif()

  VkToy_supports_sanitizers()

  if(VkToy_ENABLE_HARDENING AND VkToy_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR VkToy_ENABLE_SANITIZER_UNDEFINED
       OR VkToy_ENABLE_SANITIZER_ADDRESS
       OR VkToy_ENABLE_SANITIZER_THREAD
       OR VkToy_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${VkToy_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${VkToy_ENABLE_SANITIZER_UNDEFINED}")
    VkToy_enable_hardening(VkToy_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(VkToy_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(VkToy_warnings INTERFACE)
  add_library(VkToy_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  VkToy_set_project_warnings(
    VkToy_warnings
    ${VkToy_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(VkToy_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    configure_linker(VkToy_options)
  endif()

  include(cmake/Sanitizers.cmake)
  VkToy_enable_sanitizers(
    VkToy_options
    ${VkToy_ENABLE_SANITIZER_ADDRESS}
    ${VkToy_ENABLE_SANITIZER_LEAK}
    ${VkToy_ENABLE_SANITIZER_UNDEFINED}
    ${VkToy_ENABLE_SANITIZER_THREAD}
    ${VkToy_ENABLE_SANITIZER_MEMORY})

  set_target_properties(VkToy_options PROPERTIES UNITY_BUILD ${VkToy_ENABLE_UNITY_BUILD})

  if(VkToy_ENABLE_PCH)
    target_precompile_headers(
      VkToy_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(VkToy_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    VkToy_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(VkToy_ENABLE_CLANG_TIDY)
    VkToy_enable_clang_tidy(VkToy_options ${VkToy_WARNINGS_AS_ERRORS})
  endif()

  if(VkToy_ENABLE_CPPCHECK)
    VkToy_enable_cppcheck(${VkToy_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(VkToy_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    VkToy_enable_coverage(VkToy_options)
  endif()

  if(VkToy_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(VkToy_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(VkToy_ENABLE_HARDENING AND NOT VkToy_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR VkToy_ENABLE_SANITIZER_UNDEFINED
       OR VkToy_ENABLE_SANITIZER_ADDRESS
       OR VkToy_ENABLE_SANITIZER_THREAD
       OR VkToy_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    VkToy_enable_hardening(VkToy_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()

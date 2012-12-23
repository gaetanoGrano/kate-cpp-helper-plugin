#
# Find clang C API library
#
# Copyright 2012 by Alex Turbov <i.zaufi@gmail.com>
#
# kate: hl cmake;

include(FindPackageHandleStandardArgs)

# NOTE In gentoo clang library placed into /usr/lib64/llvm,
# so to find it `llvm-config` required
# But in Ubuntu 12.10 libclang.so placed into /usr/lib...
find_program(
    LLVM_CONFIG_EXECUTABLE
    llvm-config
  )
if (LLVM_CONFIG_EXECUTABLE)
    message(STATUS "Found LLVM configuration tool: ${LLVM_CONFIG_EXECUTABLE}")

    # Get LLVM library dir
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
        OUTPUT_VARIABLE LLVM_LIBDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
endif()

# Try to find libclang.so
find_library(
    LIBCLANG_LIBRARY
    clang
    PATH ${LLVM_LIBDIR}
  )
# Try to compile sample test which would output clang version
try_run(
    _clang_get_version_run_result
    _clang_get_version_compile_result
    ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/modules/libclang-get-version.cpp
    COMPILE_DEFINITIONS ${LLVM_CXXFLAGS}
    CMAKE_FLAGS -DLINK_LIBRARIES:STRING=${LIBCLANG_LIBRARY}
    COMPILE_OUTPUT_VARIABLE _clang_get_version_compile_output
    RUN_OUTPUT_VARIABLE _clang_get_version_run_output
  )
# Extract clang version. In my system this string look like:
# "clang version 3.1 (branches/release_31)"
# In Ubuntu 12.10 it looks like:
# "Ubuntu clang version 3.0-6ubuntu3 (tags/RELEASE_30/final) (based on LLVM 3.0)"
string(
    REGEX
    REPLACE ".*clang version ([23][^ \\-]+)[ \\-].*" "\\1"
    LIBCLANG_VERSION
    "${_clang_get_version_run_output}"
  )

message(STATUS "Found Clang C API version ${LIBCLANG_VERSION}")

find_package_handle_standard_args(
    LibClang
    REQUIRED_VARS LIBCLANG_LIBRARY
    VERSION_VAR LIBCLANG_VERSION
  )
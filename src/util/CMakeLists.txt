file(WRITE ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "\#include \"rose_paths.h\"\n\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* Paths derived from the CMake source/build/install roots. */\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_SOURCE_TREE           = \"${ROSE_TOP_SRC_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_TREE            = \"${ROSE_TOP_BINARY_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_PREFIX        = \"${CMAKE_INSTALL_PREFIX}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_INCLUDE_DIR   = \"${INCLUDE_INSTALL_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_LIB_DIR       = \"${LIB_INSTALL_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_CLANG_INCLUDE_DIR = \"${CLANG_BUILTIN_HEADERS_INSTALL_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR = \"${ROSE_TOP_BINARY_DIR}/include-staging\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_LIB_DIR         = \"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\";\n")
if(NOT DEFINED ROSE_CONFIGURE_DATE)
  string(TIMESTAMP ROSE_CONFIGURE_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)
  set(ROSE_CONFIGURE_DATE "${ROSE_CONFIGURE_DATE}" CACHE INTERNAL
      "Time of the initial REX configuration")
endif()
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_CONFIGURE_DATE     = \"${ROSE_CONFIGURE_DATE}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_OS           = \"${CMAKE_HOST_SYSTEM_VERSION}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_CPU          = \"${CMAKE_HOST_SYSTEM_PROCESSOR}\";\n")
configure_file(
  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp
  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C COPYONLY)

configure_file(
  ${ROSE_TOP_SRC_DIR}/src/util/rose_paths.h
  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.h COPYONLY)

# needed to properly define ROSE_UTIL_API
add_definitions(-DROSE_UTIL_EXPORTS)

add_subdirectory(commandlineProcessing)
add_subdirectory(graphs)
add_subdirectory(support)

add_library(util_main OBJECT
  ${CMAKE_BINARY_DIR}/src/util/rose_paths.C
  FileSystem.C
  mlog.C
  processSupport.C
  rose_path_resolver.C)

add_library(roseUtil INTERFACE)
target_sources(roseUtil INTERFACE
  $<TARGET_OBJECTS:util_main>
  $<TARGET_OBJECTS:util_commandlineProcessing>
  $<TARGET_OBJECTS:util_support>
  $<TARGET_OBJECTS:util_graphs>)

install(FILES
  processSupport.h rose_paths.h mlog.h ROSE_ASSERT.h ROSE_ABORT.h ROSE_DEPRECATED.h ROSE_FALLTHROUGH.h
  rose_path_resolver.h rose_test_output_path.h
  DESTINATION ${INCLUDE_INSTALL_DIR})

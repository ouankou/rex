######## Generate rose_paths.C #########

# Simplifies variable reference and escape sequence evaluation in the following file generating commands.
cmake_policy(SET CMP0053 NEW)

file(REMOVE ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp)
file(WRITE  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "\#include <string> \n\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* Use the same header file to declare these variables as is used to reference them so that they will be globally available (and not local). */\n")
file(APPEND  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "\#include \"rose_paths.h\" \n\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* Paths derived from the CMake source/build/install roots. */\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_GFORTRAN_PATH          = \"${CMAKE_Fortran_COMPILER}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_SOURCE_TREE           = \"${ROSE_TOP_SRC_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_TREE            = \"${ROSE_TOP_BINARY_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_PREFIX        = \"${CMAKE_INSTALL_PREFIX}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_INCLUDE_DIR   = \"${INCLUDE_INSTALL_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_INSTALL_CLANG_INCLUDE_DIR = \"${CLANG_BUILTIN_HEADERS_INSTALL_DIR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR = \"${ROSE_TOP_BINARY_DIR}/include-staging\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_LIB_DIR         = \"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "\n/* Additional interesting data to provide. */\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_CONFIGURE_DATE     = \"@configure_date@\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_OS           = \"${CMAKE_HOST_SYSTEM_VERSION}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_BUILD_CPU          = \"${CMAKE_HOST_SYSTEM_PROCESSOR}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "const std::string ROSE_OFP_VERSION_STRING = \"${enable-ofp-version}\";\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* Numeric form of ROSE version -- assuming ROSE version x.y.zL \(where */\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* x, y, and z are numbers, and L is a single lowercase letter from a to j\), */\n")
file(APPEND ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp "/* the numeric value is x * 1000000 + y * 10000 + z * 100 + \(L - 'a'\). */\n")

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C.tmp
    ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.C)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${ROSE_TOP_SRC_DIR}/src/util/rose_paths.h
  ${ROSE_TOP_BINARY_DIR}/src/util/rose_paths.h)

# needed to properly define ROSE_UTIL_API
add_definitions(-DROSE_UTIL_EXPORTS)

######## build sub libraries ##########
add_subdirectory(commandlineProcessing)
add_subdirectory(graphs)
add_subdirectory(StringUtility)
add_subdirectory(support)

######## build main library ###########
set(rose_util_src
  ${CMAKE_BINARY_DIR}/src/util/rose_paths.C
  FileSystem.C
  Unique.C
  mlog.C
  rose_getline.C
  processSupport.C
  rose_getline.C
  rose_strtoull.C)

add_library(util_main OBJECT ${rose_util_src})
add_dependencies(util_main generate_rosePublicConfig)

add_library(roseUtil INTERFACE)
target_sources(roseUtil INTERFACE
  $<TARGET_OBJECTS:util_main>
  $<TARGET_OBJECTS:util_commandlineProcessing>
  $<TARGET_OBJECTS:util_StringUtility>
  $<TARGET_OBJECTS:util_support>
  $<TARGET_OBJECTS:util_graphs>)
if(SQLITE3_FOUND_LIB)
  target_link_libraries(roseUtil INTERFACE ${SQLITE3_LIBRARY})
endif()

########### install files ###############
install(FILES
  processSupport.h rose_paths.h mlog.h ROSE_ASSERT.h ROSE_ABORT.h ROSE_DEPRECATED.h ROSE_UNUSED.h ROSE_FALLTHROUGH.h FileSystem.h
  Unique.h timing.h rose_getline.h rose_strtoull.h rose_isnan.h
  rose_pragma_message.h
  DESTINATION ${INCLUDE_INSTALL_DIR})

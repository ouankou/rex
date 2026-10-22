include_directories(
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/transformationTracking
  ${CMAKE_SOURCE_DIR}/src/util
  ${CMAKE_SOURCE_DIR}/src/frontend
  ${ROSE_INCLUDES})

# Keep these lists aligned with ROSETTA's generated outputs.
set(ROSETTA_HEADERS
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/AstQueryMemoryPool.h
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.h
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarDeclarations.h
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarVariants.h
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarTreeTraversalAccessEnums.h
)

# ROSETTA-generated source files.
set(ROSETTA_SRC
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_Grammar.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarCopyMemberFunctions.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarNewAndDeleteOperators.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarNewConstructors.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarProcessDataMemberReferenceToPointers.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarReturnClassHierarchySubTree.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarReturnDataMemberPointers.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarRTI.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarTraverseMemoryPool.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarTreeTraversalSuccessorContainer.C
  ${CMAKE_BINARY_DIR}/src/frontend/SageIII/Cxx_GrammarVariantEnumNames.C
)

set_source_files_properties(${ROSETTA_SRC} ${ROSETTA_HEADERS} PROPERTIES GENERATED 1)
add_subdirectory(util)
add_subdirectory(Rose)
add_subdirectory(ROSETTA)

add_definitions(-DROSE_DLL_EXPORTS)

target_sources(roseUtil INTERFACE
  $<TARGET_OBJECTS:rose_StringUtility>)

add_subdirectory(frontend)
add_subdirectory(midend)
add_subdirectory(backend)
add_subdirectory(AstNodes)

foreach(_rose_sage_pch_target IN ITEMS
  abstractLayer
  astDiagnostics
  astDump
  astPostProcessing
  astProcessing
  astTokenStream
  includeDirectivesProcessing
  midend
  midend_loopproc
  midend_pa
  midend_pt
  midend_util
  roseAstNodesExpression
  sage3
  sageInterface
  trans_tracking
  unparser
  virtualCFG)
  if(TARGET ${_rose_sage_pch_target})
    rose_enable_sage_pch(${_rose_sage_pch_target})
  endif()
endforeach()
unset(_rose_sage_pch_target)

set(rose_LIB_SRCS
  ${ROSETTA_SRC}
  $<TARGET_OBJECTS:util_main>
  $<TARGET_OBJECTS:util_commandlineProcessing>
  $<TARGET_OBJECTS:rose_StringUtility>
  $<TARGET_OBJECTS:util_support>
  $<TARGET_OBJECTS:util_graphs>
  $<TARGET_OBJECTS:unparser>
  $<TARGET_OBJECTS:sage3>
  $<TARGET_OBJECTS:roseAstNodesExpression>
  $<TARGET_OBJECTS:virtualCFG>
  $<TARGET_OBJECTS:midend>
  $<TARGET_OBJECTS:abstractLayer>
  $<TARGET_OBJECTS:astDiagnostics>
  $<TARGET_OBJECTS:astProcessing>
  $<TARGET_OBJECTS:midend_util>
  $<TARGET_OBJECTS:midend_pt>
  $<TARGET_OBJECTS:ompparser>
  $<TARGET_OBJECTS:sageInterface>
  $<TARGET_OBJECTS:astTokenStream>
  $<TARGET_OBJECTS:astDump>
  $<TARGET_OBJECTS:includeDirectivesProcessing>
  $<TARGET_OBJECTS:astPostProcessing>
  $<TARGET_OBJECTS:trans_tracking>)

if(TARGET accparser)
  list(APPEND rose_LIB_SRCS $<TARGET_OBJECTS:accparser>)
endif()

list(APPEND rose_LIB_SRCS
  $<TARGET_OBJECTS:midend_pa>
  $<TARGET_OBJECTS:midend_loopproc>)

# Add Clang frontend objects (always enabled for REX when C is enabled)
if(ENABLE-C)
  list(APPEND rose_LIB_SRCS
    $<TARGET_OBJECTS:libroseClangFrontend>)
endif()

if(ENABLE-FORTRAN)
  list(APPEND rose_LIB_SRCS
    $<TARGET_OBJECTS:libroseFlangFrontend>)
endif()

if(REX_ENABLE_WASM)
  add_library(ROSE_DLL STATIC ${rose_LIB_SRCS})
else()
  add_library(ROSE_DLL SHARED ${rose_LIB_SRCS})
endif()

add_dependencies(ROSE_DLL roseUtil)
target_sources(ROSE_DLL PRIVATE
  docs/Attributes.h
  docs/mrdocs/ast_node_docs.h
  docs/mrdocs/doc_prelude.h
  docs/mrdocs/modules/ast_diagnostics.h
  docs/mrdocs/modules/ast_inlining.h
  docs/mrdocs/modules/ast_processing.h
  docs/mrdocs/modules/backend.h
  docs/mrdocs/modules/backend_generator.h
  docs/mrdocs/modules/cxx_code_generator.h
  docs/mrdocs/modules/fortran_code_generator.h
  docs/mrdocs/modules/frontend.h
  docs/mrdocs/modules/frontend_sage_interface.h
  docs/mrdocs/modules/language_independent_code_generator.h
  docs/mrdocs/modules/midend.h
  docs/mrdocs/modules/program_transformation.h
  docs/mrdocs/modules/query_lib.h
  docs/mrdocs/modules/query_lib_details.h
  docs/mrdocs/rose/ast_construction.h
  docs/mrdocs/rose/default_constructor_destructor.h
  docs/mrdocs/rose/developers.h
  docs/mrdocs/rose/heap_object_ownership.h
  docs/mrdocs/rose/how_to_document.h
  docs/mrdocs/rose/main_page.h
  docs/mrdocs/sage/sage_groups.h)

set_target_properties(ROSE_DLL PROPERTIES OUTPUT_NAME "rose")

# Create namespaced alias for modern CMake usage (build tree only).
add_library(Rose::rose ALIAS ROSE_DLL)
message(STATUS "Created alias target Rose::rose")

# Propagate include directories to consumers (build and install trees).
target_include_directories(ROSE_DLL PUBLIC
  $<BUILD_INTERFACE:${ROSE_TOP_SRC_DIR}/src>
  $<BUILD_INTERFACE:${ROSE_TOP_SRC_DIR}/src/frontend/SageIII>
  $<BUILD_INTERFACE:${ROSE_TOP_SRC_DIR}/src/Rose>
  $<BUILD_INTERFACE:${ROSE_TOP_BINARY_DIR}>
  $<BUILD_INTERFACE:${ROSE_TOP_BINARY_DIR}/src/util>
  $<BUILD_INTERFACE:${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII>
  $<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>
  $<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/Rose>
)

# Public headers use C++17 facilities, including std::filesystem.
target_compile_features(ROSE_DLL PUBLIC cxx_std_17)

add_dependencies(ROSE_DLL rosetta_generated)
rose_enable_sage_pch(ROSE_DLL)

# Link Clang/LLVM libraries (required for REX).
# Keep these in the public link interface because GNU ld does not resolve
# undefined symbols from ROSE_DLL through its DT_NEEDED entries alone when
# downstream executables link against librose.
if(ENABLE-C)
  target_link_libraries(ROSE_DLL
    PUBLIC ${link_with_libraries} ${CLANG_LIBS} ${LLVM_LIBS})
else()
  target_link_libraries(ROSE_DLL PUBLIC ${link_with_libraries} ${LLVM_LIBS})
endif()

if(ENABLE-FORTRAN)
  target_link_libraries(ROSE_DLL PRIVATE ${FLANG_LIBS})
endif()

set_target_properties(ROSE_DLL PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
  DEFINE_SYMBOL ROSE_DLL_EXPORTS
  EXPORT_NAME rose)

# Install the ROSE library with export information for downstream CMake usage.
install(TARGETS ROSE_DLL
  EXPORT RoseTargets
  LIBRARY DESTINATION ${LIB_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  RUNTIME DESTINATION ${BIN_INSTALL_DIR}
  INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
)

install(EXPORT RoseTargets
  FILE RoseTargets.cmake
  NAMESPACE Rose::
  DESTINATION ${LIB_INSTALL_DIR}/cmake/Rose
)
install(
  FILES
    featureTests.h rose.h roseInternal.h
    ${CMAKE_BINARY_DIR}/rosePublicConfig.h
  DESTINATION ${INCLUDE_INSTALL_DIR})

# Dependencies of the ROSE library. librose depends on additional libraries.

set(rose_library_dir_list "")
macro(add_library_dependency lib)
  get_filename_component(lib_dir ${lib} DIRECTORY)
  list(APPEND rose_library_dir_list ${lib_dir})
endmacro()


# Zlib is a compression library used by libharu.
if(NOT REX_ENABLE_WASM)
  target_link_libraries(ROSE_DLL PRIVATE ZLIB::ZLIB)
  add_library_dependency(${ZLIB_LIBRARY})
endif()

if(TARGET REX::HPDF)
  target_link_libraries(ROSE_DLL PRIVATE REX::HPDF)
endif()
if(HPDF_LIBRARY_DIRS)
  list(APPEND rose_library_dir_list ${HPDF_LIBRARY_DIRS})
endif()

set(LINK_FILES
  ${CMAKE_DL_LIBS})

foreach(_rose_embedded_parser_target ompparser accparser)
  if(TARGET ${_rose_embedded_parser_target})
    get_target_property(_rose_embedded_parser_link_libraries
      ${_rose_embedded_parser_target} LINK_LIBRARIES)
    if(_rose_embedded_parser_link_libraries)
      list(APPEND LINK_FILES ${_rose_embedded_parser_link_libraries})
    endif()
    unset(_rose_embedded_parser_link_libraries)
  endif()
endforeach()
unset(_rose_embedded_parser_target)

target_link_libraries(ROSE_DLL PRIVATE ${LINK_FILES})
if(TARGET rose_clang_libstdcxx_headers)
  target_link_libraries(ROSE_DLL PUBLIC rose_clang_libstdcxx_headers)
endif()
# If the C preprocessor is enabled, ROSE has some additional depenendencies
if(ENABLE-CPP)
  add_dependencies(ROSE_DLL ROSE_PREPROCESSOR)
endif()

remove_definitions(-DROSE_DLL_EXPORTS)

if(NOT REX_ENABLE_WASM)

# The rose-config tool (Deprecated; use the rose-config.cfg file directly instead)

add_executable(rose-config rose-config.C)
target_link_libraries(rose-config ROSE_DLL ${link_with_libraries} ${CMAKE_DL_LIBS})
install(TARGETS rose-config DESTINATION bin)

# The rose-compiler tool

add_executable(rose-compiler rose-compiler.C)
target_link_libraries(rose-compiler ROSE_DLL ${link_with_libraries})
install(TARGETS rose-compiler DESTINATION bin)

endif()

if(REX_ENABLE_WASM)
  add_subdirectory(wasm)
endif()


# rose-config.cfg -- text file that contains various settings for user-level makefiles.

#Build LDFLAGS for ROSE tools
set(rose_config_installed_cppflags "-I${CMAKE_INSTALL_PREFIX}/include/rose ")

list(APPEND rose_library_dir_list "${CMAKE_INSTALL_PREFIX}/${ROSE_LIB_DIR_NAME}")
list(REMOVE_DUPLICATES rose_library_dir_list)
list(JOIN rose_library_dir_list ":" ROSE_LIBDIRS)
set(ROSE_LINK_RPATHS "")
set(ROSE_RPATHS "")

foreach(X IN LISTS rose_library_dir_list)
  set(rose_config_installed_ldflags "${rose_config_installed_ldflags} -L${X}")
  set(ROSE_LINK_RPATHS "${ROSE_LINK_RPATHS} -Wl,-rpath ${X}")
  set(ROSE_RPATHS "${ROSE_RPATHS} -R ${X}")
endforeach()
string(APPEND rose_config_installed_ldflags " -lrose -lm")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "rose_library_dir_list: ${rose_library_dir_list}")
  message(STATUS "ROSE_LIBDIRS: ${ROSE_LIBDIRS}")
  message(STATUS "ROSE_LINK_RPATHS: ${ROSE_LINK_RPATHS}")
  message(STATUS "ROSE_RPATHS: ${ROSE_RPATHS}")
endif()

file(WRITE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rose-config.cfg
  "# Things that the 'rose-config' tool can report\n"
  "ROSE_CC           = ${BACKEND_C_COMPILER_NAME_WITHOUT_PATH}\n"
  "ROSE_CCPATH       = ${CMAKE_C_COMPILER}\n"
  "ROSE_CXX          = ${BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH}\n"
  "ROSE_CXX_PATH     = ${CMAKE_CXX_COMPILER}\n"
  "ROSE_CPPFLAGS     = ${rose_config_installed_cppflags}\n"
  "ROSE_CFLAGS       = ${CMAKE_C_FLAGS}\n"
  "ROSE_CXXFLAGS     = ${CMAKE_CXX_FLAGS}\n"
  "ROSE_LDFLAGS      = ${rose_config_installed_ldflags}\n"
  "ROSE_LIBDIRS      = ${ROSE_LIBDIRS}\n"
  "ROSE_PREFIX       = ${CMAKE_INSTALL_PREFIX}\n"
  "# Additional items useful to include in Makefiles\n"
  "ROSE_RPATHS       = ${ROSE_RPATHS}\n"
  "ROSE_LINK_RPATHS  = ${ROSE_LINK_RPATHS}\n"
  "ROSE_VERSION      = ${ROSE_PACKAGE_VERSION}\n"
  "ROSE_BACKEND_C_COMPILER      = ${BACKEND_C_COMPILER}\n"
  "ROSE_BACKEND_CXX_COMPILER    = ${BACKEND_CXX_COMPILER}\n")
install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rose-config.cfg DESTINATION ${LIB_INSTALL_DIR})

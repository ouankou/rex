set(TestUnparseHeaders_SRCS
    TestUnparseHeaders.C
    UnparseHeadersTransformVisitor.C)
add_executable(TestUnparseHeaders ${TestUnparseHeaders_SRCS})
target_link_libraries(TestUnparseHeaders ROSE_DLL ${link_with_libraries} )

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)

set(_rose_switches_base "-w -rose:verbose 0 -rose:unparseHeaderFiles -c")
set(_outliner_switches
  "-rose:outline:use_dlopen -rose:outline:copy_orig_file -rose:outline:temp_variable -rose:outline:exclude_headers")
set(_hdr_root_switches "-rose:unparseHeaderFilesRootFolder unparsedHeaders")

function(rex_get_unparse_headers_source out_var spec)
  file(GLOB_RECURSE _source_candidates
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${spec}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${spec}/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/${spec}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/${spec}/*.cpp")
  list(LENGTH _source_candidates _source_candidate_count)
  if(_source_candidate_count EQUAL 0)
    message(FATAL_ERROR
      "UnparseHeaders test '${spec}' does not contain a C++ source file")
  elseif(_source_candidate_count GREATER 1)
    message(FATAL_ERROR
      "UnparseHeaders test '${spec}' contains multiple C++ source files: "
      "${_source_candidates}")
  endif()
  list(GET _source_candidates 0 _source_rel)
  set(${out_var} "${_source_rel}" PARENT_SCOPE)
endfunction()

set(_test_specs
  test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11
  test12 test13 test14 test15 test18 test19 test20)

foreach(spec IN LISTS _test_specs)
  rex_get_unparse_headers_source(_source_rel ${spec})
  get_filename_component(_source_name "${_source_rel}" NAME)
  rex_get_cxx_specimen_extra_flags(_extra_frontend_flags ${_source_name})
  string(JOIN " " _extra_frontend_switches ${_extra_frontend_flags})
  set(_rose_switches
    "${_rose_switches_base} ${_extra_frontend_switches} ${_outliner_switches} -rose:applicationRootDirectory ${CMAKE_CURRENT_SOURCE_DIR}/${spec}")
  set(_fixture "unparse_headers_${spec}_clean_tree")

  add_test(
    NAME unparse_headers_prepare_${spec}
    COMMAND /bin/sh -c
      "rm -rf \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\" \"${CMAKE_CURRENT_BINARY_DIR}/_rose_unparsed_headers_\" && \
       mkdir -p \"${CMAKE_CURRENT_BINARY_DIR}/${spec}\""
  )

  add_test(
    NAME unparse_headers_${spec}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      "ROSE_SWITCHES=${_rose_switches}"
      "HDR_ROOT_SWITCHES=${_hdr_root_switches}"
      "TEST_UNPARSE_HEADERS=$<TARGET_FILE:TestUnparseHeaders>"
      ${CMAKE_CURRENT_SOURCE_DIR}/${spec}/config
      ${spec}.passed
  )

  set_tests_properties(unparse_headers_prepare_${spec}
    PROPERTIES
      FIXTURES_SETUP ${_fixture}
      LABELS UNPARSEHEADERTEST)
  set_tests_properties(unparse_headers_${spec}
    PROPERTIES
      DEPENDS unparse_headers_prepare_${spec}
      FIXTURES_REQUIRED ${_fixture}
      LABELS UNPARSEHEADERTEST)
endforeach()

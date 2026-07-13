include_directories(${ROSE_INCLUDES})

set(_callgraph_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_callgraph_output_dir})

add_executable(testCG CG.C)
target_link_libraries(testCG ROSE_DLL ${link_with_libraries})

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/testCallGraph.C)
  add_executable(testCallGraph testCallGraph.C)
  target_link_libraries(testCallGraph ROSE_DLL ${link_with_libraries})
else()
  message(WARNING "Missing testCallGraph.C in testCallGraphAnalysis.")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ClassHierarchyTestGen.cpp)
  add_executable(classHierarchyTestGen ClassHierarchyTestGen.cpp)
  target_link_libraries(classHierarchyTestGen ROSE_DLL ${link_with_libraries})
else()
  message(WARNING "Missing ClassHierarchyTestGen.cpp in testCallGraphAnalysis.")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/VirtualFctsTester.cpp)
  add_executable(virtualFctsTester VirtualFctsTester.cpp)
  target_link_libraries(virtualFctsTester ROSE_DLL ${link_with_libraries})
else()
  message(WARNING "Missing VirtualFctsTester.cpp in testCallGraphAnalysis.")
endif()

add_executable(testNewCallGraph testNewCallGraph.C)
target_link_libraries(testNewCallGraph ROSE_DLL ${link_with_libraries})

add_executable(rex_callgraph_implicit_conversion
  rex_callgraph_implicit_conversion.cpp)
target_link_libraries(rex_callgraph_implicit_conversion
  ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_test01_spec_dir ${CMAKE_CURRENT_SOURCE_DIR}/test01-specimens)
set(_test01_sources
  ${_test01_spec_dir}/main.C
  ${_test01_spec_dir}/f1.C
  ${_test01_spec_dir}/f2.C
)
set(_quoted_sources "")
foreach(source IN LISTS _test01_sources)
  list(APPEND _quoted_sources "\"${source}\"")
endforeach()
string(JOIN " " _test01_sources_arg ${_quoted_sources})
set(_test01_cmd
  "$<TARGET_FILE:testCG> -I. -I${_test01_spec_dir} ${_test01_sources_arg} -o Callgraph"
)

add_test(
  NAME callgraph_test01
  WORKING_DIRECTORY ${_callgraph_output_dir}
  COMMAND ${_rth}
    CMD=${_test01_cmd}
    ${_exit_status}
    test01.passed
)

set(_test02_conf ${CMAKE_CURRENT_SOURCE_DIR}/test02.conf)
foreach(n IN ITEMS 10 100)
  add_test(
    NAME callgraph_test02_${n}
    WORKING_DIRECTORY ${_callgraph_output_dir}
    COMMAND ${_rth}
      N=${n}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      TESTCG=$<TARGET_FILE:testCG>
      ${_test02_conf}
      test02_${n}.passed
  )
endforeach()

set(_test03_specimens
  test01.C
  test02.C
  test03.C
  test04.C
  test05.C
  test06.C
  test07.C
  test08.C
  test09.C
  test10.C
)
set(_test03_conf ${CMAKE_CURRENT_SOURCE_DIR}/test03.conf)
set(_test03_answers ${CMAKE_CURRENT_SOURCE_DIR}/test03-answers)

foreach(specimen IN LISTS _test03_specimens)
  string(REPLACE "." "_" _name ${specimen})
  get_filename_component(_base ${specimen} NAME_WE)
  add_test(
    NAME callgraph_test03_${_name}
    WORKING_DIRECTORY ${_callgraph_output_dir}
    COMMAND ${_rth}
      INPUT=${specimen}
      OUTPUT=${_base}.o
      ANSWERS=${_test03_answers}
      TESTCG=$<TARGET_FILE:testCG>
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      top_srcdir=${CMAKE_SOURCE_DIR}
      ${_test03_conf}
      test03_${_name}.passed
  )
endforeach()

file(GLOB _test04_answer_files
  "${CMAKE_CURRENT_SOURCE_DIR}/test04-answers/*.C.cg.dmp")
set(_test04_conf ${CMAKE_CURRENT_SOURCE_DIR}/test04.conf)
set(_test04_answers ${CMAKE_CURRENT_SOURCE_DIR}/test04-answers)
set(_test04_spec_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
foreach(answer_file IN LISTS _test04_answer_files)
  get_filename_component(_answer_name ${answer_file} NAME)
  string(REPLACE ".cg.dmp" "" _specimen ${_answer_name})
  if(NOT EXISTS "${_test04_spec_dir}/${_specimen}")
    message(STATUS
      "Skipping callgraph test04 answer without source: ${_specimen}")
    continue()
  endif()
  string(REPLACE "." "_" _name ${_specimen})
  get_filename_component(_base ${_specimen} NAME_WE)
  rex_get_cxx_specimen_standard_flag(_callgraph_standard_flag ${_specimen})
  add_test(
    NAME callgraph_test04_${_name}
    WORKING_DIRECTORY ${_callgraph_output_dir}
    COMMAND ${_rth}
      INPUT=${_specimen}
      OUTPUT=${_base}.o
      ANSWERS=${_test04_answers}
      TESTCG=$<TARGET_FILE:testCG>
      CXX_STANDARD_FLAG=${_callgraph_standard_flag}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      top_srcdir=${CMAKE_SOURCE_DIR}
      ${_test04_conf}
      test04_${_name}.passed
  )
endforeach()

add_test(
  NAME callgraph_new_01
  WORKING_DIRECTORY ${_callgraph_output_dir}
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_01.c
)

add_test(
  NAME callgraph_new_02
  WORKING_DIRECTORY ${_callgraph_output_dir}
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_02.c
)

add_test(
  NAME callgraph_new_03
  WORKING_DIRECTORY ${_callgraph_output_dir}
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_03.C
)

add_test(
  NAME rex_callgraph_implicit_conversion
  WORKING_DIRECTORY ${_callgraph_output_dir}
  COMMAND $<TARGET_FILE:rex_callgraph_implicit_conversion> -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_callgraph_implicit_conversion_input.cpp
)
set_tests_properties(rex_callgraph_implicit_conversion PROPERTIES
  LABELS "rex;CALLGRAPH;CXX")

get_property(_callgraph_registered_tests DIRECTORY PROPERTY TESTS)
if(_callgraph_registered_tests)
  set_property(TEST ${_callgraph_registered_tests} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_callgraph_output_dir}")
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set_tests_properties(${_callgraph_registered_tests} PROPERTIES
      PROCESSORS 16)
  endif()
endif()

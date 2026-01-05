include_directories(${ROSE_INCLUDES})

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

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_callgraph_test01_enabled FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.2")
  set(_callgraph_test01_enabled TRUE)
endif()

set(_callgraph_test04_enabled FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
  set(_callgraph_test04_enabled TRUE)
endif()

set(_test01_spec_dir ${CMAKE_CURRENT_SOURCE_DIR}/test01-specimens)
set(_test01_sources
  ${_test01_spec_dir}/main.C
  ${_test01_spec_dir}/f1.C
  ${_test01_spec_dir}/f2.C
)

add_test(
  NAME callgraph_test01
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    CMD="$<TARGET_FILE:testCG> -I. -I${_test01_spec_dir} ${_test01_sources} -o Callgraph"
    ${_exit_status}
    test01.passed
)
if(NOT _callgraph_test01_enabled)
  set_tests_properties(callgraph_test01 PROPERTIES DISABLED TRUE)
endif()

set(_test02_conf ${CMAKE_CURRENT_SOURCE_DIR}/test02.conf)
foreach(n IN ITEMS 10 100)
  add_test(
    NAME callgraph_test02_${n}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      N=${n}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      INPUT=${specimen}
      OUTPUT=${_base}.o
      ANSWERS=${_test03_answers}
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
  string(REPLACE "." "_" _name ${_specimen})
  get_filename_component(_base ${_specimen} NAME_WE)
  add_test(
    NAME callgraph_test04_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      INPUT=${_specimen}
      OUTPUT=${_base}.o
      ANSWERS=${_test04_answers}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      top_srcdir=${CMAKE_SOURCE_DIR}
      ${_test04_conf}
      test04_${_name}.passed
  )
  if(NOT _callgraph_test04_enabled)
    set_tests_properties(callgraph_test04_${_name} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

add_test(
  NAME callgraph_new_01
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_01.c
)
set_tests_properties(callgraph_new_01 PROPERTIES DISABLED TRUE)

add_test(
  NAME callgraph_new_02
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_02.c
)
set_tests_properties(callgraph_new_02 PROPERTIES DISABLED TRUE)

add_test(
  NAME callgraph_new_03
  COMMAND testNewCallGraph -c ${CMAKE_CURRENT_SOURCE_DIR}/newCallGraph_input_03.C
)
set_tests_properties(callgraph_new_03 PROPERTIES DISABLED TRUE)

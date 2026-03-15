set(CXX20_TESTCODES
  test2020_02.C
  test2020_05.C
  test2020_07.C
  test2020_10.C
  test2020_11.C
  test2020_12.C
  test2020_13.C
  test2020_14.C
  test2020_21.C
  test2020_22.C
  test2020_24.C
  test2020_25.C
  test2020_26.C
  test2020_28.C
  test2020_29.C
  test2020_31.C
  test2020_32.C
  test2020_33.C
  test2020_34.C
  test2020_35.C
  test2020_36.C
  test2020_38.C
  test2020_41.C
  test2020_42.C
  test2020_43.C
  test2020_44.C
  test2020_45.C
  test2020_46.C
  test2020_47.C
  test2020_48.C
  test2020_49.C
  test2020_50.C
  test2020_51.C
  test2020_52.C
  test2020_53.C
  test2020_54.C
  test2020_55.C
  test2020_56.C
  test2020_57.C
  test2020_58.C
  test2020_59.C
  test2020_60.C
  test2020_61.C
  test2020_62.C
  test2020_63.C
  test2020_64.C
  test2020_65.C
  test2020_66.C
  test2020_67.C
  test2020_68.C
  test2020_75.C
  test2020_82.C
  test2020_99.C
  test2020_105.C
  test2020_106.C
  test2020_107.C
  test2020_108.C
  test2020_109.C
  test2020_112.C
  test2020_113.C
  test2020_117.C
  test2020_123.C
  trivial.C
  test2020_01.C
  test2020_04.C
  test2020_06.C
  test2020_08.C
  test2020_09.C
  test2020_15.C
  test2020_16.C
  test2020_17.C
  test2020_18.C
  test2020_19.C
  test2020_20.C
  test2020_23.C
  test2020_27.C
  test2020_30.C
  test2020_37.C
  test2020_39.C
  test2020_40.C
  test2020_69.C
  test2020_70.C
  test2020_71.C
  test2020_72.C
  test2020_73.C
  test2020_74.C
  test2020_76.C
  test2020_77.C
  test2020_78.C
  test2020_79.C
  test2020_80.C
  test2020_81.C
  test2020_83.C
  test2020_84.C
  test2020_85.C
  test2020_86.C
  test2020_87.C
  test2020_88.C
  test2020_89.C
  test2020_90.C
  test2020_91.C
  test2020_92.C
  test2020_94.C
  test2020_95.C
  test2020_96.C
  test2020_97.C
  test2020_98.C
  test2020_100.C
  test2020_101.C
  test2020_102.C
  test2020_103.C
  test2020_104.C
  test2020_110.C
  test2020_111.C
  test2020_114.C
  test2020_115.C
  test2020_116.C
  test2020_118.C
  test2020_120.C
  test2020_122.C
  test2020_124.C
  test2020_125.C
  test2020_126.C
  test2020_03.C
  test2020_93.C
  test2020_119.C
  test2020_121.C
  rex_test2026_concepts_requires.cpp
)
set(CXX20_DISABLED_TESTCODES
  test2020_05.C
  test2020_10.C
  test2020_12.C
  test2020_13.C
  test2020_21.C
  test2020_22.C
  test2020_26.C
  test2020_32.C
  test2020_33.C
  test2020_34.C
  test2020_35.C
  test2020_36.C
  test2020_41.C
  test2020_75.C
  test2020_82.C
  test2020_99.C
  test2020_113.C
  test2020_117.C
  test2020_123.C
)
set(CXX20_EXPECTED_FAIL_TESTCODES
  test2020_47.C
)

list(APPEND REX_FRONTEND_ERROR_TESTS
  test2020_46.C
  test2020_49.C
  test2020_56.C
  test2020_59.C
  test2020_66.C
)

set(ROSE_SKIP_FINAL_COMPILE_TESTCODES
  test2020_46.C
  test2020_49.C
  test2020_56.C
  test2020_59.C
  test2020_66.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++20)

set(ROSE_DISABLED_TESTCODES ${CXX20_DISABLED_TESTCODES})
set(ROSE_EXPECTED_FAIL_TESTCODES ${CXX20_EXPECTED_FAIL_TESTCODES})

foreach(file_to_test ${CXX20_TESTCODES})
  if(file_to_test STREQUAL "test2020_45.C")
    continue()
  endif()
  compile_test(${file_to_test} "Cxx20_tests")
  rex_compile_test_name(_test_name "Cxx20_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
endforeach()

set(_test2020_45_source "${CMAKE_CURRENT_SOURCE_DIR}/test2020_45.C")
set(_test2020_45_module_source "${CMAKE_CURRENT_SOURCE_DIR}/test2020_45_module.C")
set(_test2020_45_module_dir "${CMAKE_CURRENT_BINARY_DIR}/test2020_45_modules")
set(_test2020_45_iostream_pcm "${_test2020_45_module_dir}/iostream.pcm")
set(_test2020_45_module_pcm "${_test2020_45_module_dir}/test2020_45_module.pcm")

string(CONCAT _test2020_45_command
  "set -eu; "
  "module_dir=\"${_test2020_45_module_dir}\"; "
  "mkdir -p \"$module_dir\"; "
  "${CMAKE_CXX_COMPILER} -std=c++20 -xc++-system-header --precompile iostream "
  "-o \"${_test2020_45_iostream_pcm}\"; "
  "${CMAKE_CXX_COMPILER} -std=c++20 -x c++-module --precompile "
  "\"${_test2020_45_module_source}\" "
  "-fmodule-file=\"${_test2020_45_iostream_pcm}\" "
  "-o \"${_test2020_45_module_pcm}\"; "
  "translator=\"$1\"; shift; "
  "exec \"$translator\" "
  "-fmodule-file=\"${_test2020_45_iostream_pcm}\" "
  "-fmodule-file=test2020_45_module=${_test2020_45_module_pcm} "
  "\"$@\"")

add_test(
  NAME Cxx20_tests_test2020_45_C
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "${_test2020_45_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_test2020_45_source}
)
set_tests_properties(Cxx20_tests_test2020_45_C PROPERTIES LABELS Cxx20_tests)
rex_lock_test_for_input(Cxx20_tests_test2020_45_C test2020_45.C)

# rex_test2026: Concepts/constraints require C++20 and must round-trip cleanly.
string(CONCAT rex_test2026_concepts_requires_unparse_command
  "\"$@\" && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2026_concepts_requires.cpp"
)
add_test(
  NAME Cxx20_tests_rex_test2026_concepts_requires_unparse
  COMMAND
    sh -c
    "${rex_test2026_concepts_requires_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_concepts_requires.cpp
)
set_tests_properties(Cxx20_tests_rex_test2026_concepts_requires_unparse PROPERTIES LABELS Cxx20_tests )
rex_lock_test_for_input(Cxx20_tests_rex_test2026_concepts_requires_unparse rex_test2026_concepts_requires.cpp)

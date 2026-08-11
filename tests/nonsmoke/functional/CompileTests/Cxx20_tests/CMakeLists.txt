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
  rex_cxx20_char8_type_identity.cpp
  rex_frontend_dependent_builtin_bit_cast.cpp
  rex_frontend_constrained_overload_identity.cpp
  rex_frontend_range_init_declarator_identity.cpp
  rex_test2026_coreturn_valgrind_defined_reads.cpp
  rex_test2026_cxx20_paren_list_init_valgrind_defined_reads.cpp
  rex_test2026_concepts_requires.cpp
  rex_unparser_asm_clobber_identity.cpp
  rex_unparser_semantic_roundtrip.cpp
  rex_unparser_structural_qualified_template_uses.cpp
)

set(CXX20_NEGATIVE_TESTCODES
  test2020_47.C
  test2020_46.C
  test2020_49.C
  test2020_56.C
  test2020_59.C
  test2020_66.C
  # Clang 22 rejects the original as an ambiguous call in C++20 mode.
  test2020_93.C
)
set(CXX20_POSITIVE_TESTCODES ${CXX20_TESTCODES})
list(REMOVE_ITEM CXX20_POSITIVE_TESTCODES ${CXX20_NEGATIVE_TESTCODES})

rex_register_compile_source_set(REX_CXX20_TESTS_SOURCES ${CXX20_TESTCODES})
rex_register_compile_source_set(REX_CXX20_TESTS_NEGATIVE_SOURCES
  ${CXX20_NEGATIVE_TESTCODES}
)

set(ROSE_FLAGS -rose:verbose 0 -std=c++20)

foreach(file_to_test ${CXX20_POSITIVE_TESTCODES})
  if(file_to_test STREQUAL "test2020_45.C")
    continue()
  endif()
  compile_test(${file_to_test} "Cxx20_tests")
  rex_compile_test_name(_test_name "Cxx20_tests" ${file_to_test})
  rex_lock_test_for_input(${_test_name} ${file_to_test})
endforeach()

foreach(file_to_test ${CXX20_NEGATIVE_TESTCODES})
  rex_compile_test_name(_test_name "Cxx20_tests" ${file_to_test})
  compile_negative_test(${file_to_test} "Cxx20_tests")
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

add_executable(
  rex_frontend_module_external_ownership_translator
  rex_frontend_module_external_ownership_translator.cpp
)
target_link_libraries(
  rex_frontend_module_external_ownership_translator
  ROSE_DLL
  ${link_with_libraries}
)
add_test(
  NAME Cxx20_tests_rex_frontend_module_external_ownership_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "${_test2020_45_command}"
    dummy
    $<TARGET_FILE:rex_frontend_module_external_ownership_translator>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_test2020_45_source}
)
set_tests_properties(
  Cxx20_tests_rex_frontend_module_external_ownership_structure
  PROPERTIES LABELS "rex;CFE;Cxx20_tests"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_module_external_ownership_structure
  test2020_45.C
)

set(_rex_unowned_namespace_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_unowned_imported_namespace.cpp")
set(_rex_unowned_namespace_header
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_unowned_imported_namespace.hpp")
set(_rex_unowned_namespace_module_dir
    "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_unowned_imported_namespace_modules")
set(_rex_unowned_namespace_module_pcm
    "${_rex_unowned_namespace_module_dir}/rex_frontend_unowned_imported_namespace.pcm")

string(CONCAT _rex_unowned_namespace_command
  "set -eu; "
  "module_dir=\"${_rex_unowned_namespace_module_dir}\"; "
  "mkdir -p \"$module_dir\"; "
  "${CMAKE_CXX_COMPILER} -std=c++20 -fmodule-header=user "
  "\"${_rex_unowned_namespace_header}\" "
  "-o \"${_rex_unowned_namespace_module_pcm}\"; "
  "translator=\"$1\"; shift; "
  "exec \"$translator\" "
  "-fmodule-file=\"${_rex_unowned_namespace_module_pcm}\" \"$@\"")

add_executable(
  rex_frontend_unowned_imported_namespace_translator
  rex_frontend_unowned_imported_namespace_translator.cpp
)
target_include_directories(
  rex_frontend_unowned_imported_namespace_translator
  PRIVATE ${ROSE_TOP_SRC_DIR}/src/frontend/Clang
)
rose_force_system_include_dirs(
  rex_frontend_unowned_imported_namespace_translator
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)
target_link_libraries(
  rex_frontend_unowned_imported_namespace_translator
  ROSE_DLL
  ${link_with_libraries}
)
add_test(
  NAME Cxx20_tests_rex_frontend_unowned_imported_namespace
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "${_rex_unowned_namespace_command}"
    dummy
    $<TARGET_FILE:rex_frontend_unowned_imported_namespace_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_unowned_namespace_source}
)
set_tests_properties(
  Cxx20_tests_rex_frontend_unowned_imported_namespace
  PROPERTIES LABELS "rex;CFE;Cxx20_tests"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_unowned_imported_namespace
  rex_frontend_unowned_imported_namespace.cpp
)

set(_rex_export_context_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_export_context_scope.cpp")
set(_rex_export_context_module_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_export_context_scope_module.cpp")
set(_rex_export_context_module_dir
    "${CMAKE_CURRENT_BINARY_DIR}/rex_frontend_export_context_scope_modules")
set(_rex_export_context_module_pcm
    "${_rex_export_context_module_dir}/rex_frontend_export_context_scope_module.pcm")

string(CONCAT _rex_export_context_command
  "set -eu; "
  "module_dir=\"${_rex_export_context_module_dir}\"; "
  "mkdir -p \"$module_dir\"; "
  "${CMAKE_CXX_COMPILER} -std=c++20 -x c++-module --precompile "
  "\"${_rex_export_context_module_source}\" "
  "-o \"${_rex_export_context_module_pcm}\"; "
  "translator=\"$1\"; shift; "
  "exec \"$translator\" "
  "-fmodule-file=rex_frontend_export_context_scope_module="
  "${_rex_export_context_module_pcm} \"$@\"")

add_test(
  NAME Cxx20_tests_rex_frontend_export_context_scope_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    sh -c
    "${_rex_export_context_command}"
    dummy
    $<TARGET_FILE:rex_frontend_function_publication_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_export_context_source}
)
set_tests_properties(
  Cxx20_tests_rex_frontend_export_context_scope_structure
  PROPERTIES LABELS "Cxx20_tests;rex;CFE"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_export_context_scope_structure
  rex_frontend_export_context_scope.cpp
)

# rex_test2026: Concepts/constraints require C++20 and must round-trip cleanly.
add_executable(
  rex_frontend_requires_typed_contract_translator
  rex_frontend_requires_typed_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_requires_typed_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_target_builtin_type_translator
  rex_frontend_target_builtin_type_translator.cpp
)
target_link_libraries(
  rex_frontend_target_builtin_type_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_executable(
  rex_frontend_requirement_substitution_failure_translator
  rex_frontend_requirement_substitution_failure_translator.cpp
)
target_link_libraries(
  rex_frontend_requirement_substitution_failure_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx20_tests_rex_frontend_requirement_substitution_failure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_requirement_substitution_failure_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_frontend_requirement_substitution_failure_json
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_requirement_substitution_failure.cpp
)
set_tests_properties(
  Cxx20_tests_rex_frontend_requirement_substitution_failure
  PROPERTIES LABELS "Cxx20_tests;rex;CFE;ASTJSON"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_requirement_substitution_failure
  rex_frontend_requirement_substitution_failure.cpp
)

set(_rex_target_builtin_type_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_target_builtin_type.cpp")
foreach(_rex_target_builtin_arch IN ITEMS aarch64 riscv64)
  if(_rex_target_builtin_arch STREQUAL "aarch64")
    set(_rex_target_builtin_triple aarch64-unknown-linux-gnu)
  else()
    set(_rex_target_builtin_triple riscv64-unknown-linux-gnu)
  endif()
  add_test(
    NAME Cxx20_tests_rex_frontend_target_builtin_type_${_rex_target_builtin_arch}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      $<TARGET_FILE:rex_frontend_target_builtin_type_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
      -rex:ast-json-checkpoint=pre-omp-construction
      -rex:ast-json-dir=rex_frontend_target_builtin_type_${_rex_target_builtin_arch}_json
      -target ${_rex_target_builtin_triple}
      -c ${_rex_target_builtin_type_source}
  )
  set_tests_properties(
    Cxx20_tests_rex_frontend_target_builtin_type_${_rex_target_builtin_arch}
    PROPERTIES LABELS "Cxx20_tests;rex;CFE;ASTJSON"
  )
  rex_lock_test_for_input(
    Cxx20_tests_rex_frontend_target_builtin_type_${_rex_target_builtin_arch}
    rex_frontend_target_builtin_type.cpp
  )
endforeach()

add_executable(
  rex_frontend_constrained_overload_identity_translator
  rex_frontend_constrained_overload_identity_translator.cpp
)
target_link_libraries(
  rex_frontend_constrained_overload_identity_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx20_tests_rex_frontend_constrained_overload_identity
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_constrained_overload_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:skip_unparse
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_constrained_overload_identity.cpp
)
set_tests_properties(
  Cxx20_tests_rex_frontend_constrained_overload_identity
  PROPERTIES LABELS "Cxx20_tests;rex;CFE"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_constrained_overload_identity
  rex_frontend_constrained_overload_identity.cpp
)

add_executable(
  rex_frontend_coroutine_ast_json_contract_translator
  rex_frontend_coroutine_ast_json_contract_translator.cpp
)
target_link_libraries(
  rex_frontend_coroutine_ast_json_contract_translator
  ROSE_DLL
  ${link_with_libraries}
)

foreach(_rex_coroutine_source test2020_42.C test2020_43.C test2020_44.C)
  get_filename_component(
    _rex_coroutine_stem
    "${_rex_coroutine_source}"
    NAME_WE
  )
  set(
    _rex_coroutine_test
    "Cxx20_tests_rex_frontend_coroutine_ast_json_contract_${_rex_coroutine_stem}"
  )
  add_test(
    NAME ${_rex_coroutine_test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND
      $<TARGET_FILE:rex_frontend_coroutine_ast_json_contract_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep
      -rex:ast-json-checkpoint=pre-omp-construction
      -rex:ast-json-dir=rex_frontend_coroutine_ast_json_contract_${_rex_coroutine_stem}_json
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${_rex_coroutine_source}
  )
  set_tests_properties(
    ${_rex_coroutine_test}
    PROPERTIES LABELS "Cxx20_tests;rex;CFE;ASTJSON"
  )
  rex_lock_test_for_input(
    ${_rex_coroutine_test}
    ${_rex_coroutine_source}
  )
endforeach()

string(CONCAT rex_test2026_concepts_requires_unparse_command
  "\"$@\" && "
  "grep -Eq 'SameType[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]+auto[[:space:]]+constrained_auto_value' rose_rex_test2026_concepts_requires.cpp && "
  "grep -Eq 'SameType[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]+auto[[:space:]]+constrained_auto_result' rose_rex_test2026_concepts_requires.cpp && "
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

add_test(
  NAME Cxx20_tests_rex_frontend_requires_typed_contract
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_requires_typed_contract_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_frontend_requires_typed_contract_json
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2026_concepts_requires.cpp
)
set_tests_properties(
  Cxx20_tests_rex_frontend_requires_typed_contract
  PROPERTIES LABELS "Cxx20_tests;rex;CFE"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_requires_typed_contract
  rex_test2026_concepts_requires.cpp
)

set(_rex_cxx20_char8_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_cxx20_char8_type_identity.cpp")
set(_rex_cxx20_char8_output "rose_rex_cxx20_char8_type_identity.cpp")
string(CONCAT _rex_cxx20_char8_command
  "set -eu; "
  "\"$@\"; "
  "grep -Eq 'distinguish[[:space:]]*\\([[:space:]]*char8_t' "
  "${_rex_cxx20_char8_output}; "
  "grep -Eq 'distinguish[[:space:]]*\\([[:space:]]*unsigned[[:space:]]+char' "
  "${_rex_cxx20_char8_output}; "
  "${CMAKE_CXX_COMPILER} -std=c++20 ${_rex_cxx20_char8_output} "
  "-o rex_cxx20_char8_type_identity; "
  "./rex_cxx20_char8_type_identity"
)
add_test(
  NAME Cxx20_tests_rex_cxx20_char8_type_identity_unparse
  COMMAND
    sh -c
    "${_rex_cxx20_char8_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_cxx20_char8_source}
)
set_tests_properties(
  Cxx20_tests_rex_cxx20_char8_type_identity_unparse
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_cxx20_char8_type_identity_unparse
  rex_cxx20_char8_type_identity.cpp
)

set(_rex_unparser_semantic_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_semantic_roundtrip.cpp")
set(_rex_unparser_semantic_output
    "rose_rex_unparser_semantic_roundtrip.cpp")

string(CONCAT _rex_unparser_semantic_command
  "set -eu; "
  "\"$@\"; "
  "grep -Eq '\\.real[[:space:]]*=[[:space:]]*3\\.5' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'initializer_list[[:space:]]+pair[[:space:]]*\\([[:space:]]*3[[:space:]]*,[[:space:]]*4[[:space:]]*\\)' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq '__assert_fail[[:space:]]*==[[:space:]]*17' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'reference_identity[[:space:]]*\\([[:space:]]*41' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'reference_identity[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\([[:space:]]*42' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'reference_owner[[:space:]]*\\.[[:space:]]*identity[[:space:]]*\\([[:space:]]*43' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'reference_owner[[:space:]]*\\.[[:space:]]*identity[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\([[:space:]]*44' "
  "${_rex_unparser_semantic_output}; "
  "! grep -q '__PRETTY_FUNCTION__' ${_rex_unparser_semantic_output}; "
  "${CMAKE_CXX_COMPILER} -std=c++20 ${_rex_unparser_semantic_output} "
  "-o rex_unparser_semantic_roundtrip; "
  "./rex_unparser_semantic_roundtrip"
)
add_test(
  NAME Cxx20_tests_rex_unparser_semantic_roundtrip
  COMMAND
    sh -c
    "${_rex_unparser_semantic_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_unparser_semantic_source}
)
set_tests_properties(
  Cxx20_tests_rex_unparser_semantic_roundtrip
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_unparser_semantic_roundtrip
  rex_unparser_semantic_roundtrip.cpp
)

add_executable(
  rex_unparser_ast_roundtrip_translator
  rex_unparser_ast_roundtrip_translator.cpp
)
target_link_libraries(
  rex_unparser_ast_roundtrip_translator
  ROSE_DLL
  ${link_with_libraries}
)

string(CONCAT _rex_unparser_ast_command
  "set -eu; "
  "if ! \"$@\" >rex_unparser_ast_roundtrip.log 2>&1; then "
  "cat rex_unparser_ast_roundtrip.log; exit 1; fi; "
  "! grep -q 'consult bug 320' rex_unparser_ast_roundtrip.log; "
  "grep -Eq 'builder_value[[:space:]]*=[[:space:]]*6\\.50*F' "
  "${_rex_unparser_semantic_output}; "
  "! grep -Eq 'builder_value[[:space:]]*=[[:space:]]*6\\.50*L' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'Derived[[:space:]]*:[^{}]*Alias' "
  "${_rex_unparser_semantic_output}; "
  "grep -Eq 'Dependent[[:space:]]*:[^{}]*T' "
  "${_rex_unparser_semantic_output}; "
  "${CMAKE_CXX_COMPILER} -std=c++20 ${_rex_unparser_semantic_output} "
  "-o rex_unparser_ast_roundtrip; "
  "./rex_unparser_ast_roundtrip"
)
add_test(
  NAME Cxx20_tests_rex_unparser_ast_roundtrip
  COMMAND
    sh -c
    "${_rex_unparser_ast_command}"
    dummy
    $<TARGET_FILE:rex_unparser_ast_roundtrip_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -rex:ast-json-checkpoint=pre-omp-construction
    -rex:ast-json-dir=rex_unparser_ast_roundtrip_json
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_unparser_semantic_source}
)
set_tests_properties(
  Cxx20_tests_rex_unparser_ast_roundtrip
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_unparser_ast_roundtrip
  rex_unparser_semantic_roundtrip.cpp
)

add_test(
  NAME Cxx20_tests_rex_unparser_missing_operator_metadata_is_rejected
  COMMAND
    sh -c
    "ulimit -c 0; set +e; REX_TEST_MISSING_OPERATOR_METADATA=1 \"$@\" >rex_unparser_missing_operator_metadata.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Ec '^REX_UNPARSE_INVARIANT\\[operator-source-callee\\]: call=0x[[:xdigit:]]+ surface=[[:digit:]]+ resolves to a declaration of the wrong kind$' rex_unparser_missing_operator_metadata.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_ast_roundtrip_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_unparser_semantic_source}
)
set_tests_properties(
  Cxx20_tests_rex_unparser_missing_operator_metadata_is_rejected
  PROPERTIES LABELS "Cxx20_tests;rex;UNPARSER;death"
)
rex_lock_test_for_input(
  Cxx20_tests_rex_unparser_missing_operator_metadata_is_rejected
  rex_unparser_semantic_roundtrip.cpp
)

add_executable(
  rex_unparser_unrecognized_register
  rex_unparser_unrecognized_register.cpp
)
target_link_libraries(
  rex_unparser_unrecognized_register
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx20_tests_rex_unparser_unrecognized_register
  COMMAND
    sh -c
    "ulimit -c 0; set +e; \"$1\" >rex_unparser_unrecognized_register.log 2>&1; status=$?; set -e; test $status -eq 134; test \"\$(grep -Fxc 'REX_UNPARSE_INVARIANT[asm-clobber]: invalid character in asm name' rex_unparser_unrecognized_register.log)\" -eq 1"
    dummy
    $<TARGET_FILE:rex_unparser_unrecognized_register>
)
set_tests_properties(
  Cxx20_tests_rex_unparser_unrecognized_register
  PROPERTIES LABELS "Cxx20_tests;death"
)

set(_rex_unparser_asm_clobber_source
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_unparser_asm_clobber_identity.cpp")
set(_rex_unparser_asm_clobber_output
    "rose_rex_unparser_asm_clobber_identity.cpp")
string(CONCAT _rex_unparser_asm_clobber_command
  "set -eu; "
  "\"$@\"; "
  "grep -F '\"rax\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F '\"eax\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F '\"ax\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F '\"al\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F '\"cc\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F '\"memory\"' ${_rex_unparser_asm_clobber_output}; "
  "grep -F 'asm (\"rex_unparser_exact_symbol\")' ${_rex_unparser_asm_clobber_output}"
)
add_test(
  NAME Cxx20_tests_rex_unparser_asm_clobber_identity
  COMMAND
    sh -c
    "${_rex_unparser_asm_clobber_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -target x86_64-unknown-linux-gnu
    -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${_rex_unparser_asm_clobber_source}
)
set_tests_properties(
  Cxx20_tests_rex_unparser_asm_clobber_identity
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_unparser_asm_clobber_identity
  rex_unparser_asm_clobber_identity.cpp
)

add_test(
  NAME Cxx20_tests_rex_frontend_range_init_declarator_identity_structure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_range_init_declarator_identity.cpp
)
set_tests_properties(
  Cxx20_tests_rex_frontend_range_init_declarator_identity_structure
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_range_init_declarator_identity_structure
  rex_frontend_range_init_declarator_identity.cpp
)

add_test(
  NAME Cxx20_tests_rex_frontend_range_init_declarator_identity_token_mapping
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND
    $<TARGET_FILE:rex_frontend_variable_declarator_identity_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep -rose:unparse_tokens
    ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_frontend_range_init_declarator_identity.cpp
)
set_tests_properties(
  Cxx20_tests_rex_frontend_range_init_declarator_identity_token_mapping
  PROPERTIES LABELS Cxx20_tests
)
rex_lock_test_for_input(
  Cxx20_tests_rex_frontend_range_init_declarator_identity_token_mapping
  rex_frontend_range_init_declarator_identity.cpp
)

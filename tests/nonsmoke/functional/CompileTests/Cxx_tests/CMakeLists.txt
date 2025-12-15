# this defines ROSE__CXX_TESTS and ROSE__CXX_TESTS__BOOST
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

include(Cxx_Testcodes.cmake)
include(Cxx_Failing_Tests.cmake)

set(CXX_TESTCODES_TO_RUN ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})

set(ROSE_FLAGS
  --edg:no_warnings -w -rose:verbose 0
  -rose:skip_unparse_asm_commands
  -rose:skip_unparse_cc_commands
  -I${CMAKE_CURRENT_SOURCE_DIR}/../A++Code
  )

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
    compile_test(${file_to_test} "Cxx_tests_failing")
  endforeach()
endif()

foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "Cxx_tests")
endforeach()

# Issue 91: Declaration chains must not emit "firstNondefiningDeclaration == NULL"
set_tests_properties(Cxx_tests_rex_test2025_issue91_decl_chain PROPERTIES
  FAIL_REGULAR_EXPRESSION "firstNondefiningDeclaration == NULL"
)

# Issue 92: Must not emit "Redundant symbol removed...from symbol table"
set_tests_properties(Cxx_tests_rex_test2025_issue92_redundant_sym PROPERTIES
  FAIL_REGULAR_EXPRESSION "Redundant symbol removed"
)

# Issue 123: Function refs must not emit duplicated qualification (e.g. std::::get).
add_test(
  NAME Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  COMMAND
    sh -c
    "\"$@\" && ! grep -E \"::::\" rose_rex_test2025_issue123_std_double_colon.cpp && grep -E \"std::get[[:space:]]*<0>\" rose_rex_test2025_issue123_std_double_colon.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue123_std_double_colon.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue123_std_double_colon.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue123_std_double_colon_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 124: System header template specializations must preserve template args.
add_test(
  NAME Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"Use<.*(::)?std::vector[[:space:]]*<\" rose_rex_test2025_issue124_system_header_specialization_args.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue124_system_header_specialization_args.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue124_system_header_specialization_args.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue124_system_header_specialization_args_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 125: Declarations must not be missing from their owning scope child list.
set_tests_properties(Cxx_tests_rex_test2025_issue125_fixup_child_list_warning PROPERTIES
  FAIL_REGULAR_EXPRESSION "not in child list of scope"
)

# Issue 126: Default template args must be printed from IR; A<> must not unparse
# as "template A a;".
add_test(
  NAME Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"template[[:space:]]*<[[:space:]]*(class|typename)[[:space:]]+T[[:space:]]*=[[:space:]]*void\" rose_rex_test2025_issue126_default_template_args.cpp && ! grep -E \"template[[:space:]]+A[[:space:]]+a;\" rose_rex_test2025_issue126_default_template_args.cpp && ! grep -E \"T[[:space:]]*=[[:space:]]*void[[:space:]]*=[[:space:]]*void\" rose_rex_test2025_issue126_default_template_args.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue126_default_template_args.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue126_default_template_args.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue126_default_template_args_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 59: Template-template arguments must preserve required global
# qualification (no string stripping in the unparser).
add_test(
  NAME Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"UsePack[[:space:]]*<[[:space:]]*::Lib::Tuple\" rose_rex_test2025_issue59_template_template_arg_global_qual.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue59_template_template_arg_global_qual.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue59_template_template_arg_global_qual.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue59_template_template_arg_global_qual_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 64: Unparser must preserve required global qualification for template
# instantiations when a global template is shadowed by a nested-scope template.
add_test(
  NAME Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  COMMAND
    sh -c
    "\"$@\" && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue64_shadow_template_global_qual.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue64_shadow_template_global_qual.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue64_shadow_template_global_qual_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 118: EmptyDecl must translate to SgEmptyDeclaration (preserve stray ';').
set_tests_properties(Cxx_tests_rex_test2025_empty_decl PROPERTIES
  FAIL_REGULAR_EXPRESSION "No Sage node associated with the declaration"
)

# Namespace reopenings: keep declarations in their lexical namespace block.
add_test(
  NAME Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  COMMAND
    sh -c
    "\"$@\" && awk '/namespace[[:space:]]+A/ { ++ns; if (ns==1) ns1=NR; if (ns==2) ns2=NR } { if (!f && /void[[:space:]]+f[[:space:]]*\\(/) f=NR; if (!g && /void[[:space:]]+g[[:space:]]*\\(/) g=NR } END { exit !(ns2 && f && g && ns1 < f && f < ns2 && ns2 < g) }' rose_rex_test2025_namespace_reopenings_preserve_blocks.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_namespace_reopenings_preserve_blocks.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_namespace_reopenings_preserve_blocks.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_namespace_reopenings_preserve_blocks_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 93: Class-scope static_assert must preserve source order vs fields.
add_test(
  NAME Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  COMMAND sh -c "\"$@\" && awk '/struct Foo/ {inside=1} inside { if (!sa && $0 ~ /static_assert/) sa=NR; if (!x && $0 ~ /int x;/) x=NR } /};/ && inside {inside=0} END { exit !(sa && x && sa < x) }' rose_rex_test2025_issue93_static_assert_order.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue93_static_assert_order.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue93_static_assert_order.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue93_static_assert_order_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 94: Template argument lists must not introduce a space before commas.
add_test(
  NAME Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse
  COMMAND sh -c "\"$@\" && grep -F \"Container<int, 10>\" rose_rex_test2025_issue94_template_arg_spacing.cpp && ! grep -E \"Container<int[[:space:]]+,\" rose_rex_test2025_issue94_template_arg_spacing.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue94_template_arg_spacing.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue94_template_arg_spacing.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue94_template_arg_spacing_unparse PROPERTIES LABELS Cxx_tests)

# Verify explicit template arguments are preserved in calls
add_test(
  NAME Cxx_tests_rex_test2025_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep \"foo<5>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && grep \"foo<10>\" rose_rex_test2025_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_template_call_explicit_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_member_template_call_explicit_args
  COMMAND sh -c "\"$@\" && grep -E \"foo<[[:space:]]*5[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && grep -E \"foo<[[:space:]]*10[[:space:]]*>\" rose_rex_test2025_member_template_call.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_member_template_call.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_member_template_call.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_member_template_call_explicit_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_variadic_template_params_unparse
  COMMAND sh -c "\"$@\" && grep -E \"template <typename[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"template <int[[:space:]]*\\\\.\\\\.\\\\.\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && grep -E \"class[[:space:]]*\\\\.\\\\.\\\\.[[:space:]]*Templates\" rose_rex_test2025_variadic_template_params.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_variadic_template_params.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_variadic_template_params.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_variadic_template_params_unparse PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_template_argument_pack_unparse
  COMMAND sh -c "\"$@\" && grep -E \"\\\\bT[[:space:]]+t[[:space:]]*(=[[:space:]]*)?\\\\{[[:space:]]*\\\\}\" rose_rex_test2025_template_argument_pack.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_template_argument_pack.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_template_argument_pack.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_template_argument_pack_unparse PROPERTIES LABELS Cxx_tests)

# REX: This test intentionally fails (it contains a function-try-block which is currently
# not fully supported and triggers an assertion failure or error). We want to ensure
# it continues to fail (failure propagation) until properly supported.
# We add it manually here (not in Cxx_Testcodes.cmake) to avoid breaking downstream tests.
# REX: This test intentionally fails. We verify it fails (including crashes) by wrapping in shell.
# We use "$@" to pass all arguments safely to the translator command, preserving quoting.
# If "$@" returns 0 (success), we exit 1 (fail). If it returns non-zero (fail), we exit 0 (pass).
add_test(
  NAME Cxx_tests_rex_test2025_issue99_failure_propagation
  COMMAND sh -c "\"$@\" && exit 1 || exit 0" dummy $<TARGET_FILE:${translator}> ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue99_failure_propagation.cpp
)

# REX: New test for Issue 87 (Functions inside namespaces lost)
# We verify that "void bar()" is present in the output associated with namespace behavior.
add_test(
  NAME Cxx_tests_rex_test2025_issue87_namespace
  COMMAND sh -c "\"$@\" && grep \"void bar()\" rose_rex_test2025_issue87_namespace.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue87_namespace.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue87_namespace.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue87_namespace PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_issue89_type_alias_template_args
  COMMAND sh -c "\"$@\" && grep -E \"using IntContainer = Container< *int *>\" rose_rex_test2025_issue89_alias.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue89_alias.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue89_alias.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue89_type_alias_template_args PROPERTIES LABELS Cxx_tests)

add_test(
  NAME Cxx_tests_rex_test2025_issue90_array_dim_template_param
  COMMAND sh -c "\"$@\" && grep \"data\\[N\\]\" rose_rex_test2025_issue90_array_dim.cpp | grep -v \"//\" && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue90_array_dim.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue90_array_dim.cpp
)
set_tests_properties(Cxx_tests_rex_test2025_issue90_array_dim_template_param PROPERTIES LABELS Cxx_tests)

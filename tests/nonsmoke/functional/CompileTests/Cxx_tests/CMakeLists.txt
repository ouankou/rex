# this defines ROSE__CXX_TESTS and ROSE__CXX_TESTS__BOOST
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

include(Cxx_Testcodes.cmake)
include(Cxx_Failing_Tests.cmake)

set(CXX_TESTCODES_TO_RUN ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${REX_FRONTEND_ERROR_TESTS})

set(ROSE_FLAGS
  -w -rose:verbose 0
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

# Issue 99: Friend function template lookup requires error-tolerant mode today.
add_test(
  NAME Cxx_tests_rex_test2025_issue99
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rex:clang:continue-on-error ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue99.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue99
  PROPERTIES LABELS Cxx_tests
)

# Issue 91: Declaration chains must not emit "firstNondefiningDeclaration == NULL"
set_tests_properties(Cxx_tests_rex_test2025_issue91_decl_chain PROPERTIES
  FAIL_REGULAR_EXPRESSION "firstNondefiningDeclaration == NULL"
)

# Issue 92: Must not emit "Redundant symbol removed...from symbol table"
set_tests_properties(Cxx_tests_rex_test2025_issue92_redundant_sym PROPERTIES
  FAIL_REGULAR_EXPRESSION "Redundant symbol removed"
)

# Issue 145: Defaulted/deleted special members must not trigger placeholder warnings.
set_tests_properties(Cxx_tests_rex_test2025_defaulted_special_members_anon_union PROPERTIES
  FAIL_REGULAR_EXPRESSION "Cannot resolve Field member;using placeholder"
)

# Issue 150: Name-qualification map should not emit overwrite or default-scope warnings.
set_tests_properties(Cxx_tests_rex_test2025_issue150_name_qualification_map PROPERTIES
  FAIL_REGULAR_EXPRESSION "In addToNameMap\\(\\): Allow this for replacement now!;Default reached in SgScopeStatement::isNamedScope"
)

# Issue 160: Preserve file-scope static and implicit member access in unparse output.
string(CONCAT rex_test2025_issue160_axpy_roundtrip_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*static[[:space:]]+const[[:space:]]+unsigned[[:space:]]+long[[:space:]]+kElements\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "grep -E \"^[[:space:]]*static[[:space:]]+void[[:space:]]+axpy[[:space:]]*[(]\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "grep -E \"^[[:space:]]*static[[:space:]]+double[[:space:]]+checksum[[:space:]]*[(]\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"(^|[^[:alnum:]_])::Array\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"(^|[^[:alnum:]_])this([^[:alnum:]_]|$)\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \"checksum[[:space:]]*[(][[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*x[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*get_data\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "! grep -E \",[[:space:]]*[(][[:space:]]*y[[:space:]]*[.][[:space:]]*size\" rose_rex_test2025_issue160_axpy_roundtrip.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_axpy_roundtrip.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_axpy_roundtrip_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue160_axpy_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 163: Round-trip axpy.cpp must match exactly.
string(CONCAT rex_test2025_issue163_axpy_roundtrip_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue163_axpy_roundtrip.cpp "
  "rose_rex_test2025_issue163_axpy_roundtrip.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue163_axpy_roundtrip.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue163_axpy_roundtrip_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue163_axpy_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue163_axpy_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 164: Preserve explicit const on constexpr declarations.
string(CONCAT rex_test2025_issue164_constexpr_const_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp "
  "rose_rex_test2025_issue164_constexpr_const.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2025_issue164_constexpr_const.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue164_constexpr_const_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue164_constexpr_const_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue164_constexpr_const.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue164_constexpr_const_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 160: Preserve explicit qualifiers, casts, and list-init for std headers.
string(CONCAT rex_test2025_issue160_axpy_system_headers_unparse_command
  "\"$@\" && "
  "cmp -s ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_system_headers.cpp "
  "rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"constexpr[[:space:]]+std::size_t[[:space:]]+kElements\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"1u[[:space:]]*<<[[:space:]]*10\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::array<[^>]*>[[:space:]]+x[[:space:]]*\\{\\}\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::array<[^>]*>[[:space:]]+y[[:space:]]*\\{\\}\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "! grep -E \"std::iota[[:space:]]*<\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::iota[[:space:]]*\\([[:space:]]*x\\.begin\\(\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::iota[[:space:]]*\\([[:space:]]*y\\.begin\\(\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"axpy[[:space:]]*\\([[:space:]]*a[[:space:]]*,[[:space:]]*x\\.data\\(\\)[[:space:]]*,[[:space:]]*y\\.data\\(\\)[[:space:]]*,[[:space:]]*y\\.size\\(\\)[[:space:]]*\\)\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"static_cast[[:space:]]*<[^>]*double[^>]*>\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"std::fabs[[:space:]]*\\(\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"const[[:space:]]+double[[:space:]]+a[[:space:]]*=[[:space:]]*2\\.5\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "grep -E \"1e-9\" rose_rex_test2025_issue160_axpy_system_headers.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_axpy_system_headers.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_axpy_system_headers_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_axpy_system_headers.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue160_axpy_system_headers_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 160: Preserve explicit instantiation keyword for struct templates.
string(CONCAT rex_test2025_issue160_explicit_struct_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^template[[:space:]]+struct[[:space:]]+Crate<\" "
  "rose_rex_test2025_issue160_explicit_struct_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c "
  "rose_rex_test2025_issue160_explicit_struct_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_explicit_struct_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_explicit_struct_instantiation.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 160: Explicit class template instantiation must unparse as a directive.
string(CONCAT rex_test2025_issue160_explicit_class_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*template[[:space:]]+class[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*;\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+struct[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+class[[:space:]]+Box[[:space:]]*<[[:space:]]*int[[:space:]]*>[[:space:]]*\\{\" rose_rex_test2025_issue160_explicit_class_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_explicit_class_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_explicit_class_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_explicit_class_instantiation.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 160: Extern class template instantiation must unparse with "extern".
string(CONCAT rex_test2025_issue160_extern_class_instantiation_unparse_command
  "\"$@\" && "
  "grep -E \"^[[:space:]]*extern[[:space:]]+template[[:space:]]+struct[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>[[:space:]]*;\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "! grep -E \"extern[[:space:]]+template[[:space:]]+class[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "! grep -E \"template[[:space:]]+struct[[:space:]]+Holder[[:space:]]*<[[:space:]]*double[[:space:]]*>[[:space:]]*\\{\" rose_rex_test2025_issue160_extern_class_instantiation.cpp && "
  "${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue160_extern_class_instantiation.cpp"
)
add_test(
  NAME Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse
  COMMAND
    sh -c
    "${rex_test2025_issue160_extern_class_instantiation_unparse_command}"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue160_extern_class_instantiation.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue160_extern_class_instantiation_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 62: Mangled qualifiers must never contain template syntax.
set_tests_properties(Cxx_tests_rex_test2025_mangle_qualifiers_no_template_syntax PROPERTIES
  FAIL_REGULAR_EXPRESSION "In mangleQualifiersToString\\(\\):"
)

# Issue 113: Template argument spelling must not depend on using directives in
# the instantiation scope; keep namespace qualifiers in the generated name.
add_test(
  NAME Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"Container[[:space:]]*<[[:space:]]*(class|struct|union)?[[:space:]]*(::)?ns::Inner\" rose_rex_test2025_issue113_instantiation_spelling.cpp && ! grep -E \"Container[[:space:]]*<[[:space:]]*(class|struct|union)?[[:space:]]*Inner\" rose_rex_test2025_issue113_instantiation_spelling.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue113_instantiation_spelling.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue113_instantiation_spelling.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue113_instantiation_spelling_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 115: Dependent/unresolved name refs must use SgNonrealRefExp.
add_executable(
  rex_test2025_issue115_nonreal_refexp_translator
  rex_test2025_issue115_nonreal_refexp_translator.cpp
)
target_link_libraries(
  rex_test2025_issue115_nonreal_refexp_translator
  ROSE_DLL
  ${link_with_libraries}
)

foreach(
  file_to_test
  rex_test2025_issue115_unresolved_lookup.cpp
  rex_test2025_issue115_dependent_member.cpp
  rex_test2025_issue115_qualified_unresolved_lookup.cpp
)
  get_filename_component(test_name ${file_to_test} NAME_WE)
  add_test(
    NAME Cxx_tests_${test_name}_nonreal_refexp
    COMMAND
      $<TARGET_FILE:rex_test2025_issue115_nonreal_refexp_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(
    Cxx_tests_${test_name}_nonreal_refexp
    PROPERTIES LABELS Cxx_tests
  )
endforeach()

# Issue 115 Phase C: Instantiated bodies must use resolved refs.
add_executable(
  rex_test2025_issue115_phaseC_resolution_translator
  rex_test2025_issue115_phaseC_resolution_translator.cpp
)
target_link_libraries(
  rex_test2025_issue115_phaseC_resolution_translator
  ROSE_DLL
  ${link_with_libraries}
)

foreach(
  file_to_test
  rex_test2025_issue115_unresolved_lookup.cpp
  rex_test2025_issue115_dependent_member.cpp
  rex_test2025_issue115_qualified_unresolved_lookup.cpp
)
  get_filename_component(test_name ${file_to_test} NAME_WE)
  add_test(
    NAME Cxx_tests_${test_name}_phaseC_resolution
    COMMAND
      $<TARGET_FILE:rex_test2025_issue115_phaseC_resolution_translator>
      ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test}
  )
  set_tests_properties(
    Cxx_tests_${test_name}_phaseC_resolution
    PROPERTIES LABELS Cxx_tests
  )
endforeach()

# Issue 147: Dependent template types must use SgNonrealType.
add_executable(
  rex_test2025_dependent_template_specialization_type_translator
  rex_test2025_dependent_template_specialization_type_translator.cpp
)
target_link_libraries(
  rex_test2025_dependent_template_specialization_type_translator
  ROSE_DLL
  ${link_with_libraries}
)

add_test(
  NAME Cxx_tests_rex_test2025_dependent_template_specialization_type_nonreal_type
  COMMAND
    $<TARGET_FILE:rex_test2025_dependent_template_specialization_type_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_dependent_template_specialization_type.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_dependent_template_specialization_type_nonreal_type
  PROPERTIES LABELS Cxx_tests
)

# Issue 148: Mutating system-header declarations must preserve AST invariants.
add_executable(
  rex_test2025_issue148_system_header_mutation_translator
  rex_test2025_issue148_system_header_mutation_translator.cpp
)
target_link_libraries(
  rex_test2025_issue148_system_header_mutation_translator
  ROSE_DLL
  ${link_with_libraries}
)

set(rex_test2025_issue148_system_header_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/../../input_codes/system_header_like)
add_test(
  NAME Cxx_tests_rex_test2025_issue148_system_header_mutation
  COMMAND
    $<TARGET_FILE:rex_test2025_issue148_system_header_mutation_translator>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -isystem ${rex_test2025_issue148_system_header_dir}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue148_system_header_mutation.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue148_system_header_mutation
  PROPERTIES LABELS Cxx_tests
)

# Issue 151: LinkageSpecDecl (extern "C") must not drop declarations and must
# preserve linkage in the generated output.
add_test(
  NAME Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -F 'extern \"C\"' rose_rex_test2025_extern_c_roundtrip.cpp && grep -E '(^|[^[:alnum:]_])int[[:space:]]+foo[[:space:]]*[(]' rose_rex_test2025_extern_c_roundtrip.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_extern_c_roundtrip.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_extern_c_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_extern_c_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 151: Preserve extern "C" brace form without flattening.
add_test(
  NAME Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -F 'extern \"C\" {' rose_rex_test2025_extern_c_brace_roundtrip.cpp && grep -E '(^|[^[:alnum:]_])int[[:space:]]+foo[[:space:]]*[(]' rose_rex_test2025_extern_c_brace_roundtrip.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_extern_c_brace_roundtrip.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_extern_c_brace_roundtrip.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_extern_c_brace_roundtrip_unparse
  PROPERTIES LABELS Cxx_tests
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

# Issue 146: Namespace-scope template instantiations must not emit std::::iota.
add_test(
  NAME Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  COMMAND
    sh -c
    "\"$@\" && ! grep -F '::::' rose_rex_test2025_issue146_std_iota_quad_colon.cpp && grep -E '(^|[^[:alnum:]_])std::iota[[:space:]]*(<|\\()' rose_rex_test2025_issue146_std_iota_quad_colon.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue146_std_iota_quad_colon.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue146_std_iota_quad_colon.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue146_std_iota_quad_colon_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 146: Global function template instantiations must keep :: under shadowing.
add_test(
  NAME Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E '(^|[^[:alnum:]_])::make_tag' rose_rex_test2025_issue146_shadowed_function_template.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_issue146_shadowed_function_template.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue146_shadowed_function_template.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue146_shadowed_function_template_unparse
  PROPERTIES LABELS Cxx_tests
)

# Issue 149: Parameter lists must not be reported as unsupported declarations.
add_test(
  NAME Cxx_tests_rex_test2025_issue149_param_list_unique_name_interface_function_coverage
  COMMAND
    $<TARGET_FILE:interfaceFunctionCoverage>
    -rose:verbose 0 -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue149_param_list_unique_name.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue149_param_list_unique_name_interface_function_coverage
  PROPERTIES
    LABELS Cxx_tests
    FAIL_REGULAR_EXPRESSION "Unsupported declaration = .*SgFunctionParameterList"
)

# Issue 144: Clang diagnostic errors must be fatal by default.
add_test(
  NAME Cxx_tests_rex_test2025_cfe_error_exit_code
  COMMAND
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_error_exit_code.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_error_exit_code
  PROPERTIES LABELS Cxx_tests WILL_FAIL TRUE
)

# Issue 144: Allow backend when explicitly opted in.
add_test(
  NAME Cxx_tests_rex_test2025_cfe_error_exit_code_continue_on_error_unparse
  COMMAND
    sh -c
    "\"$@\" && test -f rose_rex_test2025_cfe_error_exit_code.cpp && { ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_cfe_error_exit_code.cpp 2> compile.err || true; } && grep -F \"non-void function 'main' should return a value\" compile.err"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_cfe_error_exit_code.cpp
    -rex:clang:continue-on-error
)
set_tests_properties(
  Cxx_tests_rex_test2025_cfe_error_exit_code_continue_on_error_unparse
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

# Issue 60: Friend free functions declared/defined inside a class must keep
# their lexical placement (inside the class) without emitting extra namespace
# declarations/definitions.
add_test(
  NAME Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
  COMMAND
    sh -c
    "\"$@\" && grep -E \"friend[[:space:]]+int[[:space:]]+f[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && grep -E \"friend[[:space:]]+int[[:space:]]+g[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && ! grep -E \"^[[:space:]]*int[[:space:]]+f[[:space:]]*\\(\" rose_rex_test2025_issue60_friend_function_symbol_scope.cpp && ${CMAKE_CXX_COMPILER} -std=c++17 -c rose_rex_test2025_issue60_friend_function_symbol_scope.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_issue60_friend_function_symbol_scope.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_issue60_friend_function_symbol_scope_unparse
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
set_tests_properties(Cxx_tests_rex_test2025_issue118_empty_decl PROPERTIES
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

# Issue 145: Preserve explicit = default and avoid implicit-body placeholders.
add_test(
  NAME Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  COMMAND sh -c "\"$@\" && grep -F \"= default\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ! grep -F \".  operator=  .\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ! grep -F \"(this) ->  .\" rose_rex_test2025_defaulted_special_members_anon_union.cpp && ${CMAKE_CXX_COMPILER} -std=c++20 -c rose_rex_test2025_defaulted_special_members_anon_union.cpp"
    dummy
    $<TARGET_FILE:${translator}>
    ${ROSE_FLAGS} -rose:skipfinalCompileStep ${ROSE_INCLUDE_FLAGS}
    -std=c++20
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/rex_test2025_defaulted_special_members_anon_union.cpp
)
set_tests_properties(
  Cxx_tests_rex_test2025_defaulted_special_members_anon_union_unparse
  PROPERTIES LABELS Cxx_tests
  FAIL_REGULAR_EXPRESSION "Cannot resolve Field member;using placeholder"
)

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

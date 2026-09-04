# setup ROSE_INCLUDE_FLAGS variable (used by some compile tests)
foreach(include ${ROSE_INCLUDES})
  list(APPEND ROSE_INCLUDE_FLAGS "-I${include}")
endforeach()

# default values for our compile tests
set(ROSE_FLAGS -w -rose:verbose 0)
set(translator testTranslator)

# define compile_test CMake function
function(compile_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(_test_name_prefix "${label}")
  if(DEFINED REX_COMPILE_TEST_NAME_PREFIX)
    set(_test_name_prefix "${REX_COMPILE_TEST_NAME_PREFIX}")
  endif()
  set(test_id ${_test_name_prefix}_${_test_key})
  set(_rose_flags ${ROSE_FLAGS})
  set(_test_output_dir "${ROSE_TEST_OUTPUT_DIR}")
  if(_test_output_dir STREQUAL "")
    set(_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")
  if(DEFINED ROSE_GNU89_TESTCODES AND input_file IN_LIST ROSE_GNU89_TESTCODES)
    # Allow per-test GNU89-based C89 dialect classification without changing global defaults.
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags "-std=gnu89")
  endif()
  if(input_file MATCHES "\\.(C|cc|cpp|cxx)$")
    set(_has_cxx_standard FALSE)
    foreach(_rose_flag IN LISTS _rose_flags)
      if(_rose_flag MATCHES "^-std=(c|gnu)\\+\\+")
        set(_has_cxx_standard TRUE)
        break()
      endif()
    endforeach()
    if(NOT _has_cxx_standard)
      rex_get_cxx_specimen_standard_flag(_cxx_standard_flag ${input_file})
      list(APPEND _rose_flags ${_cxx_standard_flag})
    endif()
  endif()
  rex_get_cxx_specimen_extra_flags(_extra_rose_flags ${input_file})
  list(APPEND _rose_flags ${_extra_rose_flags})
  if(DEFINED ROSE_OUTPUT_TO_BUILD_TESTCODES AND
     input_file IN_LIST ROSE_OUTPUT_TO_BUILD_TESTCODES)
    set(_output_base "${input_file}")
    string(REPLACE "/" "_" _output_base "${_output_base}")
    list(APPEND _rose_flags "-rose:output"
         "${_test_output_dir}/rose_${_output_base}")
  endif()

  # optional argument to omit include directories
  if(ARGV2)
    add_test(
      NAME ${test_id}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${translator}
        ${_rose_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  else()
    add_test(
      NAME ${test_id}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${translator}
        ${_rose_flags} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
        ${CMAKE_CURRENT_SOURCE_DIR}/${input_file}
    )
  endif()
  set_tests_properties(${test_id} PROPERTIES LABELS "${label}")
  set_property(TEST ${test_id} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

set(REX_COMPILE_TESTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Pin every intentionally malformed C/C++ specimen to the diagnostic that
# makes it negative.  A normalized source path is the key so similarly named
# specimens in different legacy suites cannot accidentally share a contract.
function(rex_register_clang_frontend_negative_diagnostic diagnostic)
  if(ARGC LESS 2)
    message(FATAL_ERROR
      "rex_register_clang_frontend_negative_diagnostic requires a diagnostic "
      "and at least one source")
  endif()
  if(diagnostic STREQUAL "" OR diagnostic STREQUAL "--any-clang-diagnostic")
    message(FATAL_ERROR
      "A frontend-negative specimen requires a nonempty exact diagnostic")
  endif()
  string(FIND "${diagnostic}" "error:" _error_marker)
  if(_error_marker EQUAL -1)
    message(FATAL_ERROR
      "A frontend-negative diagnostic must contain a Clang error marker: "
      "'${diagnostic}'")
  endif()

  foreach(_source IN LISTS ARGN)
    if(IS_ABSOLUTE "${_source}")
      file(RELATIVE_PATH _source_key "${REX_COMPILE_TESTS_SOURCE_DIR}"
           "${_source}")
    else()
      set(_source_key "${_source}")
    endif()
    string(REPLACE "\\" "/" _source_key "${_source_key}")
    string(REGEX REPLACE "^\\./" "" _source_key "${_source_key}")
    string(MAKE_C_IDENTIFIER "${_source_key}" _property_key)
    set(_source_property
      "REX_CLANG_FRONTEND_NEGATIVE_${_property_key}_SOURCE")
    set(_diagnostic_property
      "REX_CLANG_FRONTEND_NEGATIVE_${_property_key}_DIAGNOSTIC")
    get_property(_already_registered GLOBAL PROPERTY "${_source_property}" SET)
    if(_already_registered)
      get_property(_existing_source GLOBAL PROPERTY "${_source_property}")
      message(FATAL_ERROR
        "Duplicate or colliding frontend-negative diagnostic keys for "
        "'${_existing_source}' and '${_source_key}'")
    endif()
    set_property(GLOBAL PROPERTY "${_source_property}" "${_source_key}")
    set_property(GLOBAL PROPERTY "${_diagnostic_property}" "${diagnostic}")
  endforeach()
endfunction()

function(rex_get_clang_frontend_negative_diagnostic output_variable source)
  if(IS_ABSOLUTE "${source}")
    file(RELATIVE_PATH _source_key "${REX_COMPILE_TESTS_SOURCE_DIR}" "${source}")
  else()
    set(_source_key "${source}")
  endif()
  string(REPLACE "\\" "/" _source_key "${_source_key}")
  string(REGEX REPLACE "^\\./" "" _source_key "${_source_key}")
  string(MAKE_C_IDENTIFIER "${_source_key}" _property_key)
  set(_source_property "REX_CLANG_FRONTEND_NEGATIVE_${_property_key}_SOURCE")
  set(_diagnostic_property
    "REX_CLANG_FRONTEND_NEGATIVE_${_property_key}_DIAGNOSTIC")
  get_property(_registered GLOBAL PROPERTY "${_source_property}" SET)
  if(NOT _registered)
    message(FATAL_ERROR
      "No exact Clang frontend-negative diagnostic registered for "
      "'${_source_key}'")
  endif()
  get_property(_registered_source GLOBAL PROPERTY "${_source_property}")
  if(NOT _registered_source STREQUAL _source_key)
    message(FATAL_ERROR
      "Frontend-negative diagnostic key collision between "
      "'${_registered_source}' and '${_source_key}'")
  endif()
  get_property(_diagnostic GLOBAL PROPERTY "${_diagnostic_property}")
  if(_diagnostic STREQUAL "" OR
     _diagnostic STREQUAL "--any-clang-diagnostic")
    message(FATAL_ERROR
      "Invalid frontend-negative diagnostic registered for '${_source_key}'")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
endfunction()

# C and C++ compile-suite negatives.  Diagnostics deliberately omit the source
# prefix so the same contract also applies when instrumentation copies a C
# specimen to a uniquely named temporary file.
rex_register_clang_frontend_negative_diagnostic(
  [=[error: unknown type name 'bool']=]
  C89_std_c89_tests/test2013_06.c
  C_tests/test2013_06.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[fatal error: 'sse_debug.h' file not found]=]
  C99_tests/complex_02.c)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: a type specifier is required for all declarations]=]
  C++Code/test2003_29.C
  ElsaTestCases/gnu/g0018.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: unknown type name 'x']=]
  C++Code/test2004_20.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: unknown type name 'SerialArray_Domain_Type']=]
  C++Code/test2004_116.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'SpaceConcept']=]
  C++Code/test2005_41.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier '__USE_BSD']=]
  C++Code/test2005_50.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: explicit instantiation of 'std::basic_string' must occur in namespace '__cxx11']=]
  C++Code/test2005_57.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: too many template arguments for class template '_Rb_tree_iterator']=]
  C++Code/test2005_98.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: template specialization requires 'template<>']=]
  C++Code/test2005_135.C
  C++Code/test2005_138.C
  C++Code/test2005_156.C
  ElsaTestCases/gnu/bugs/gb0012.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: declaration does not declare anything]=]
  C++Code/test2005_136.C
  C++Code/test2005_139.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no template named 'Y']=]
  C++Code/test2005_149.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: friend function 'foobar' is a private member of 'Y1<X>']=]
  C++Code/test2005_173.C)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected expression]=]
  C_tests/test2011_06.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[fatal error: 'iostream' file not found]=]
  C_tests/test2011_sse_01.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: tentative definition has type 'struct timezone' that is never completed]=]
  C_tests/test2012_31.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: void function 'foobar' should not return a value]=]
  C_tests/test2012_138.c
  C_tests/test2015_93.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: incompatible pointer types initializing 'int **' with an expression of type 'int[2][2]']=]
  C_tests/test2013_50.c
  C_tests/test2013_51.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int]=]
  C_tests/test2014_03.c
  C_tests/zpagccp.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: incompatible pointer types assigning to 'struct failure_callback_t *' from 'void (*)(int) __attribute__((noreturn))']=]
  C_tests/test2014_18.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: unknown type name 'gint16']=]
  C_tests/test2014_59.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: unknown type name 'GntWidget']=]
  C_tests/test2014_75.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected ')']=]
  C_tests/test2014_90.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: incompatible pointer types passing 'struct interface ***' to parameter of type 'struct interface **'; remove &]=]
  C_tests/test2015_42.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: used type 'void *(struct numeric_resolv *)' where arithmetic or pointer type is required]=]
  C_tests/test2015_46.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: initializer element is not a compile-time constant]=]
  C_tests/test2015_87.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: incompatible integer to pointer conversion initializing 'int *' with an expression of type 'const int']=]
  C_tests/test2016_21.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: cannot redeclare builtin function '__builtin_classify_type']=]
  C_tests/test2017_02.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: conflicting types for '__builtin_ia32_maskloadpd256']=]
  C_tests/test2018_26.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: array has incomplete element type 'char[]']=]
  C_tests/test2018_52.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'a']=]
  C_tests/test2018_53.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: hex escape sequence out of range]=]
  C_tests/test2018_65.c)

# Cxx_tests itself already requires exact diagnostics.  Register the same
# source reasons here because the uninitialized-field mirror consumes its
# negative-source property independently.
rex_register_clang_frontend_negative_diagnostic(
  [=[error: invalid preprocessing directive]=]
  Cxx_tests/rex_frontend_macro_missing_separator.cpp)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: non-void function 'main' should return a value]=]
  Cxx_tests/rex_test2025_cfe_error_exit_code.cpp)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'foo']=]
  Cxx_tests/rex_test2025_issue99.cpp)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: "AST has a SgBasicBlock which has the SgIfStmt as a parent, but is not a child of the SgIfStmt IR node."]=]
  Cxx_tests/test2006_131.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no function template matches function template specialization 'operator<<']=]
  Cxx_tests/test2014_22.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: cannot redeclare builtin function '__builtin_fpclassify']=]
  Cxx_tests/test2016_35.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: template specialization requires 'template<>']=]
  Cxx_tests/test2005_69.C
  Cxx_tests/test2015_109.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: exception specification of overriding function is more lax than base version]=]
  Cxx_tests/test2015_84.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of class template 'Interval' requires template arguments]=]
  Cxx_tests/test2014_63.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no member named '_M_off' in 'fpos<_StateT>']=]
  Cxx_tests/test2016_85.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: '_M_value' is a private member of 'std::complex<double>']=]
  Cxx_tests/test2017_08.C)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: non-local lambda expression cannot have a capture-default]=]
  Cxx11_tests/rose-1973.C
  Cxx11_tests/test2019_463.C
  Cxx11_tests/test2020_69.C
  Cxx11_tests/test2020_70.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: explicit instantiation declaration (with 'extern') follows explicit instantiation definition (without 'extern')]=]
  Cxx11_tests/test2012_07.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no matching conversion for functional-style cast from 'long' to 'X<int>']=]
  Cxx11_tests/test2016_35.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no matching constructor for initialization of 'X<int>']=]
  Cxx11_tests/test2016_36.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no member named '__is_noexcept_hash' in namespace 'std::__detail']=]
  Cxx11_tests/test2018_100.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: invalid operands to binary expression ('int X::*' and 'int')]=]
  Cxx11_tests/test2019_313.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[fatal error: 'version-etc.h' file not found]=]
  Cxx11_tests/test2019_315.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected class name]=]
  Cxx11_tests/test2019_488.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: invalid operands to binary expression ('int A::*' and '__size_t' (aka 'unsigned long'))]=]
  Cxx11_tests/test2020_40.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: builtin feature check macro requires a parenthesized identifier]=]
  Cxx11_tests/test2020_54.C)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: constructor of 'A' inherited from multiple base class subobjects]=]
  Cxx17_tests/test2018_67.C)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: no matching function for call to 'f']=]
  Cxx20_tests/test2020_46.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: invalid operands to binary expression ('std::_List_iterator<int>' and 'std::_List_iterator<int>')]=]
  Cxx20_tests/test2020_47.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: a concept definition cannot refer to itself]=]
  Cxx20_tests/test2020_49.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: call to 'f1' is ambiguous]=]
  Cxx20_tests/test2020_56.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: parentheses are required around this expression in a requires clause]=]
  Cxx20_tests/test2020_59.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: call to 'h' is ambiguous]=]
  Cxx20_tests/test2020_66.C)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: call to 'f' is ambiguous]=]
  Cxx20_tests/test2020_93.C)

# ELSA compatibility specimens that Clang 22 rejects in their registered
# language modes.
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no matching function for call to 'g']=]
  ElsaTestCases/d0064.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'operator new' takes type size_t ('unsigned long') as 1st parameter]=]
  ElsaTestCases/d0091.cc
  ElsaTestCases/t0243.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use 'template' keyword to treat 'rebind' as a dependent template name]=]
  ElsaTestCases/d0112.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: reference to non-static member function must be called; did you mean to call it with no arguments?]=]
  ElsaTestCases/k0001.cc
  ElsaTestCases/notCompilable/k0001.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: extra qualification on member 'S1']=]
  ElsaTestCases/k0005.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: extra qualification on member 'varName']=]
  ElsaTestCases/k0005a.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'F' declared as an array with a negative size]=]
  ElsaTestCases/k0010.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: target exception specification is not superset of source]=]
  ElsaTestCases/k0033.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected ']']=]
  ElsaTestCases/k0042.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: cast from pointer to smaller type 'int' loses information]=]
  ElsaTestCases/k0046a.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'func' is a private member of 'A<int>']=]
  ElsaTestCases/t0244.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no member named '~B' in namespace 'N']=]
  ElsaTestCases/t0429.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: no template named 'C']=]
  ElsaTestCases/t0466.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'zerg' is a private member of 'B<int>::C']=]
  ElsaTestCases/t0516.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'x']=]
  ElsaTestCases/t0519.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: cannot define or redeclare 'f1' here because namespace 'Q' does not enclose namespace 'D']=]
  ElsaTestCases/t0539_9.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'facet' is a private member of 'std::locale']=]
  ElsaTestCases/t0546.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'f']=]
  ElsaTestCases/t0558.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier 'Min']=]
  ElsaTestCases/t0568.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: out-of-line definition of 'f' does not match any declaration in 'A<n>']=]
  ElsaTestCases/t0578.cc)

rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'type-name' cannot be signed or unsigned]=]
  ElsaTestCases/gnu/bugs/gb0002.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: ordered comparison between pointer and zero ('int (*)()' and 'int')]=]
  ElsaTestCases/gnu/bugs/gb0003.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected member name or ';' after declaration specifiers]=]
  ElsaTestCases/gnu/bugs/gb0004.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: assigning to 'State' from incompatible type 'int']=]
  ElsaTestCases/gnu/bugs/gb0006.cc)
rex_register_clang_frontend_negative_diagnostic(
  "error: '(unnamed union at ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/ElsaTestCases/gnu/d0078.cc:6:5)' cannot be defined in a type specifier"
  ElsaTestCases/gnu/d0078.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: call to '__builtin_alloca' is ambiguous]=]
  ElsaTestCases/gnu/d0089.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: void function 'parport_pc_write_data' should not return a value]=]
  ElsaTestCases/gnu/dC0005.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: array has incomplete element type 'struct board_info']=]
  ElsaTestCases/gnu/dC0006.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: expected '=' or another designator]=]
  ElsaTestCases/gnu/dC0008.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: fields must have a constant size: 'variable length array in structure' extension will never be supported]=]
  ElsaTestCases/gnu/dC0015.c)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: use of undeclared identifier '__builtin_next_arg'; did you mean '__builtin_carg'?]=]
  ElsaTestCases/gnu/g0007.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: 'Foo' cannot be defined in a type specifier]=]
  ElsaTestCases/gnu/g0013.cc)
rex_register_clang_frontend_negative_diagnostic(
  [=[error: friends can only be classes or functions]=]
  ElsaTestCases/gnu/k0004.cc)

# Exact Clang 22 contracts for the ELSA notCompilable corpus.  Legacy
# ELSA-only semantic probes are malformed Clang inputs and remain negative.
rex_register_clang_frontend_negative_diagnostic(
  "error: '(unnamed struct at ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/ElsaTestCases/notCompilable/gnu/t0125.cc:9:43)' cannot be defined in a type specifier"
  ElsaTestCases/notCompilable/gnu/t0125.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'Parent *' and 'Child *' are not pointers to compatible types]==]
  ElsaTestCases/t0132.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'B' is a private member of 'A<float>']==]
  ElsaTestCases/notCompilable/t0261.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'Frozz' cannot be defined in a parameter type]==]
  ElsaTestCases/notCompilable/t0069.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'deallocate' is a private member of '__default_alloc_template<true, 0>']==]
  ElsaTestCases/notCompilable/t0263.cc
  ElsaTestCases/notCompilable/t0264.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'f' is a private member of 'A<char>']==]
  ElsaTestCases/notCompilable/t0054.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'foo' is a private member of 'A']==]
  ElsaTestCases/notCompilable/t0177.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'getF' is a private member of 'A<int>']==]
  ElsaTestCases/notCompilable/t0287.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'hash' is a private member of 'Foo']==]
  ElsaTestCases/notCompilable/t0078.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'isatty' is missing exception specification 'throw()']==]
  ElsaTestCases/notCompilable/gnu/g0016.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'long type-name' is invalid]==]
  ElsaTestCases/notCompilable/gnu/dC0014.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'model' attribute requires a string]==]
  ElsaTestCases/notCompilable/gnu/d0099.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'operator delete[]' must have at least one parameter]==]
  ElsaTestCases/notCompilable/t0031.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'pcount' is a private member of 'strstreambuf']==]
  ElsaTestCases/notCompilable/t0099.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'seek_dir' is a private member of 'ios']==]
  ElsaTestCases/notCompilable/t0030.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'someType' is a private member of 'A<int>']==]
  ElsaTestCases/notCompilable/t0221.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'type1' is a private member of 'B']==]
  ElsaTestCases/notCompilable/t0265.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'whatever' is a private member of 'smanip<int>']==]
  ElsaTestCases/notCompilable/t0079.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: 'x' is a private member of 'Foo<float>']==]
  ElsaTestCases/notCompilable/t0036.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: ambiguous conversion from derived class 'E' to base class 'B':]==]
  ElsaTestCases/notCompilable/std/3.4.5.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: call to undeclared function 'MD5_Transform'; ISO C99 and later do not support implicit function declarations]==]
  ElsaTestCases/notCompilable/gnu/d0096.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: cannot create a non-constant pointer to member function]==]
  ElsaTestCases/notCompilable/t0408.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: cannot define or redeclare 'foo' here because namespace 'NS2' does not enclose namespace 'S']==]
  ElsaTestCases/notCompilable/k0028.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: cannot initialize return object of type 'int' with an rvalue of type 'void *']==]
  ElsaTestCases/notCompilable/t0074.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: comparison between pointer and integer ('C<3> *' and 'int')]==]
  ElsaTestCases/notCompilable/t0082.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: comparison between pointer and integer ('Foo<3> *' and 'int')]==]
  ElsaTestCases/notCompilable/t0109.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: constructor cannot be declared 'static']==]
  ElsaTestCases/notCompilable/t0062.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: declaration of reference variable 'a2' requires an initializer]==]
  ElsaTestCases/notCompilable/t0068.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: declaration of reference variable 'y' requires an initializer]==]
  ElsaTestCases/notCompilable/t0242.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: default initialization of an object of const type 'Foo *const']==]
  ElsaTestCases/notCompilable/t0012.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: default initialization of an object of const type 'const int *const volatile __restrict']==]
  ElsaTestCases/notCompilable/gnu/d0076.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: default initialization of an object of const type 'const int']==]
  ElsaTestCases/notCompilable/t0007.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: default initialization of an object of const type 'const volatile unsigned long']==]
  ElsaTestCases/notCompilable/t0072.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: exception specifications are not allowed in typedefs]==]
  ElsaTestCases/notCompilable/t0576.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: expected ';' after top level declarator]==]
  ElsaTestCases/notCompilable/gnu/d0122.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: expected ';' at end of declaration]==]
  ElsaTestCases/notCompilable/gnu/g0009.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: expected expression]==]
  ElsaTestCases/notCompilable/gnu/g0019.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: implicit default constructor for 'B<int>' must explicitly initialize the reference member 'foo']==]
  ElsaTestCases/notCompilable/t0224.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: incompatible pointer to integer conversion assigning to 'int' from 'char *']==]
  ElsaTestCases/notCompilable/t0040.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: incompatible pointer types assigning to 'int *' from 'int (*)[5]']==]
  ElsaTestCases/notCompilable/t0014.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: initialization of flexible array member is not allowed]==]
  ElsaTestCases/notCompilable/c99/t0133.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: invalid application of 'sizeof' to an incomplete type 'int[]']==]
  ElsaTestCases/notCompilable/gnu/dC0002.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: invalid use of non-static data member 'x']==]
  ElsaTestCases/notCompilable/t0216.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: member of anonymous union redeclares 'i']==]
  ElsaTestCases/notCompilable/k0034.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: must use 'struct' tag to refer to type 'ip_opts' in this scope]==]
  ElsaTestCases/notCompilable/d0041.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no candidate function template was found for dependent friend function template specialization]==]
  ElsaTestCases/notCompilable/d0103.cc
  ElsaTestCases/notCompilable/t0288.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching constructor for initialization of 'A']==]
  ElsaTestCases/notCompilable/d0053.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching function for call to 'equalTypes']==]
  ElsaTestCases/notCompilable/gnu/g0031.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching function for call to 'f']==]
  ElsaTestCases/notCompilable/t0509.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching function for call to 'foo']==]
  ElsaTestCases/notCompilable/t0271.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching function for call to 'iget4']==]
  ElsaTestCases/notCompilable/d0123.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no matching function for call to 'xxxsignal']==]
  ElsaTestCases/notCompilable/gnu/g0020.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no member named 'f' in 'S']==]
  ElsaTestCases/notCompilable/t0383.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no type named 'theType' in 'B']==]
  ElsaTestCases/notCompilable/t0215.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: non-type template argument does not refer to any declaration]==]
  ElsaTestCases/notCompilable/t0561.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: non-void function 'main' should return a value]==]
  ElsaTestCases/notCompilable/t0006.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: object expression of non-scalar type 'uintArr' (aka 'unsigned int[3]') cannot be used in a pseudo-destructor expression]==]
  ElsaTestCases/notCompilable/t0091.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: out-of-line definition of 'foo' does not match any declaration in 'A<T>']==]
  ElsaTestCases/notCompilable/t0267.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: redefinition of 'b']==]
  ElsaTestCases/notCompilable/t0251.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: template specialization requires 'template<>']==]
  ElsaTestCases/notCompilable/d0110.cc
  ElsaTestCases/notCompilable/gnu/g0025.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: type name does not allow storage class to be specified]==]
  ElsaTestCases/notCompilable/c/t0004.c
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: unknown type name 'bad_alloc'; did you mean 'std::bad_alloc'?]==]
  ElsaTestCases/notCompilable/gnu/g0023.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__cause_xfailure']==]
  ElsaTestCases/notCompilable/t0520.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__checkCalleeDefnLine']==]
  ElsaTestCases/notCompilable/t0279.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__checkType']==]
  ElsaTestCases/notCompilable/t0228.cc
  ElsaTestCases/notCompilable/t0282.cc
  ElsaTestCases/notCompilable/t0309.cc
  ElsaTestCases/notCompilable/t0310.cc
  ElsaTestCases/notCompilable/t0311.cc
  ElsaTestCases/notCompilable/t0342.cc
  ElsaTestCases/notCompilable/t0345.cc
  ElsaTestCases/notCompilable/t0533.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__computeLUB']==]
  ElsaTestCases/notCompilable/t0134.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__getImplicitConversion']==]
  ElsaTestCases/notCompilable/t0118.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__getStandardConversion']==]
  ElsaTestCases/notCompilable/t0117.cc
  ElsaTestCases/notCompilable/t0123.cc
  ElsaTestCases/notCompilable/t0209.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__testOverload']==]
  ElsaTestCases/notCompilable/std/13.3.3.1.4.cc
  ElsaTestCases/notCompilable/std/13.3.3.2a.cc
  ElsaTestCases/notCompilable/std/13.3.3.2b.cc
  ElsaTestCases/notCompilable/std/13.3.3.2d.cc
  ElsaTestCases/notCompilable/std/13.3.3b.cc
  ElsaTestCases/notCompilable/std/13.cc
  ElsaTestCases/notCompilable/std/7.3.3g.cc
  ElsaTestCases/notCompilable/std/7.3.3h.cc
  ElsaTestCases/notCompilable/std/7.3.3i.cc
  ElsaTestCases/notCompilable/std/7.3.3j.cc
  ElsaTestCases/notCompilable/t0116.cc
  ElsaTestCases/notCompilable/t0120.cc
  ElsaTestCases/notCompilable/t0127.cc
  ElsaTestCases/notCompilable/t0136.cc
  ElsaTestCases/notCompilable/t0137.cc
  ElsaTestCases/notCompilable/t0138.cc
  ElsaTestCases/notCompilable/t0139.cc
  ElsaTestCases/notCompilable/t0140.cc
  ElsaTestCases/notCompilable/t0141.cc
  ElsaTestCases/notCompilable/t0142.cc
  ElsaTestCases/notCompilable/t0143.cc
  ElsaTestCases/notCompilable/t0144.cc
  ElsaTestCases/notCompilable/t0145.cc
  ElsaTestCases/notCompilable/t0146.cc
  ElsaTestCases/notCompilable/t0148.cc
  ElsaTestCases/notCompilable/t0230.cc
  ElsaTestCases/notCompilable/t0297.cc
  ElsaTestCases/notCompilable/t0299.cc
  ElsaTestCases/notCompilable/t0354.cc
  ElsaTestCases/notCompilable/t0416.cc
  ElsaTestCases/notCompilable/t0423.cc
  ElsaTestCases/notCompilable/t0431.cc
  ElsaTestCases/notCompilable/t0529.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier '__test_mtype']==]
  ElsaTestCases/notCompilable/t0511.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: no type named 'string' in namespace 'std'; did you mean simply 'string'?]==]
  ElsaTestCases/notCompilable/d0115.cc
  ElsaTestCases/notCompilable/gnu/g0027.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: use of undeclared identifier 'var']==]
  ElsaTestCases/notCompilable/k0044.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[error: void function 'f' should not return a value]==]
  ElsaTestCases/notCompilable/k0053.cc
  ElsaTestCases/notCompilable/t0258.cc
)
rex_register_clang_frontend_negative_diagnostic(
  [==[fatal error: recursive template instantiation exceeded maximum depth of 1024]==]
  ElsaTestCases/notCompilable/d0062.cc
  ElsaTestCases/notCompilable/d0063.cc
)
set(REX_CLANG_FRONTEND_NEGATIVE_TEST_DRIVER
  "${CMAKE_CURRENT_LIST_DIR}/run_rex_clang_frontend_negative_test.sh")

function(rex_add_clang_frontend_negative_test
         test_name label working_directory log_file expected_diagnostic)
  if(expected_diagnostic STREQUAL "" OR
     expected_diagnostic STREQUAL "--any-clang-diagnostic")
    message(FATAL_ERROR
      "Frontend-negative test '${test_name}' requires an exact diagnostic")
  endif()
  string(FIND "${expected_diagnostic}" "error:" _error_marker)
  if(_error_marker EQUAL -1)
    message(FATAL_ERROR
      "Frontend-negative test '${test_name}' requires a Clang error diagnostic")
  endif()
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${working_directory}
    COMMAND ${BASH_EXECUTABLE}
      ${REX_CLANG_FRONTEND_NEGATIVE_TEST_DRIVER}
      ${log_file}
      "${expected_diagnostic}"
      --
      ${ARGN})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "${label};death")
endfunction()

# Register a Flang frontend rejection against one exact source diagnostic.
# Flang may diagnose either the original physical source or its preprocessed
# temporary copy, so require the exact specimen stem on every matching line.
function(rex_add_flang_frontend_negative_test
         test_name label working_directory log_file source_stem
         expected_diagnostic expected_count)
  if(expected_diagnostic STREQUAL "")
    message(FATAL_ERROR
      "Flang frontend-negative test '${test_name}' requires an exact diagnostic")
  endif()
  if(NOT expected_count MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "Flang frontend-negative test '${test_name}' requires a positive exact diagnostic count")
  endif()
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${working_directory}
    COMMAND ${BASH_EXECUTABLE} -c
      "ulimit -c 0; log=\"\$1\"; source_stem=\"\$2\"; expected=\"\$3\"; expected_count=\"\$4\"; shift 4; rm -f \"\$log\"; set +e; \"\$@\" >\"\$log\" 2>&1; status=\$?; set -e; test \"\$status\" -eq 1; test \"\$(grep -F -- \"\$source_stem\" \"\$log\" | sed -nE 's/^.*:[0-9]+:[0-9]+: (error:.*)$/\\1/p' | grep -Fxc -- \"error: \$expected\")\" -eq \"\$expected_count\"; test \"\$(grep -Fxc -- 'Errors in Processing Input File: throwing an instance of \"frontend_failed\" exception due to syntax errors detected in the input code ' \"\$log\")\" -eq 1"
      dummy
      ${log_file}
      ${source_stem}
      "${expected_diagnostic}"
      ${expected_count}
      ${ARGN})
  set_tests_properties(${test_name}
    PROPERTIES LABELS "${label};death")
endfunction()

function(compile_negative_test input_file label)
  rex_get_clang_frontend_negative_diagnostic(
    _expected_diagnostic "${CMAKE_CURRENT_SOURCE_DIR}/${input_file}")
  rex_compile_test_name(_test_name ${label} ${input_file})
  set(_rose_flags ${ROSE_FLAGS})
  rex_get_cxx_specimen_extra_flags(_extra_rose_flags ${input_file})
  list(APPEND _rose_flags ${_extra_rose_flags})
  string(REPLACE "/" "_" _log_key "${input_file}")
  string(REPLACE "." "_" _log_key "${_log_key}")
  set(_log_file "${_log_key}.frontend-negative.log")
  set(_test_output_dir "${ROSE_TEST_OUTPUT_DIR}")
  if(_test_output_dir STREQUAL "")
    set(_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")

  set(_include_flags ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR})
  if(ARGC GREATER 2 AND ARGV2)
    set(_include_flags "")
  endif()

  rex_add_clang_frontend_negative_test(
    ${_test_name}
    ${label}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${_log_file}
    "${_expected_diagnostic}"
    $<TARGET_FILE:${translator}>
    ${_rose_flags} ${_include_flags}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

# Register a frontend-negative specimen against the diagnostic that makes it
# intentionally invalid.  The contract cannot pass because of an unrelated
# crash, command-line error, or later backend failure.
function(compile_exact_negative_test input_file label expected_diagnostic)
  rex_compile_test_name(_test_name ${label} ${input_file})
  set(_rose_flags ${ROSE_FLAGS})
  rex_get_cxx_specimen_extra_flags(_extra_rose_flags ${input_file})
  list(APPEND _rose_flags ${_extra_rose_flags})
  string(REPLACE "/" "_" _log_key "${input_file}")
  string(REPLACE "." "_" _log_key "${_log_key}")
  set(_log_file "${_log_key}.frontend-negative.log")
  set(_test_output_dir "${ROSE_TEST_OUTPUT_DIR}")
  if(_test_output_dir STREQUAL "")
    set(_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")

  rex_add_clang_frontend_negative_test(
    ${_test_name}
    ${label}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${_log_file}
    "${expected_diagnostic}"
    $<TARGET_FILE:${translator}>
    ${_rose_flags} ${ROSE_INCLUDE_FLAGS}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
  set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

# Prevent race conditions when multiple tests process the same input file.
# Sets a CTest RESOURCE_LOCK so tests sharing the same base input name are serialized.
function(rex_lock_test_for_input test_name input_file)
  get_filename_component(lock_base ${input_file} NAME_WE)
  set_tests_properties(${test_name} PROPERTIES RESOURCE_LOCK rose_output_${lock_base})
endfunction()

# CTest permits duplicate names, which makes selection and result attribution
# ambiguous even though every underlying command remains registered.  Imported
# suites often reuse specimen basenames in different source directories, so
# assert uniqueness after the relevant child directories have been processed.
function(rex_assert_unique_ctest_names_across_directories context)
  set(_registered_test_names)
  foreach(_test_directory IN LISTS ARGN)
    get_property(_directory_test_names
      DIRECTORY "${_test_directory}" PROPERTY TESTS)
    foreach(_test_name IN LISTS _directory_test_names)
      if(_test_name IN_LIST _registered_test_names)
        message(FATAL_ERROR
          "Duplicate CTest name '${_test_name}' in ${context}")
      endif()
      list(APPEND _registered_test_names "${_test_name}")
    endforeach()
  endforeach()
endfunction()

# Compute the CTest test name that compile_test would generate for a given
# input file and label, so callers can reference it without hardcoding.
function(rex_compile_test_name out_var label input_file)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(_test_name_prefix "${label}")
  if(DEFINED REX_COMPILE_TEST_NAME_PREFIX)
    set(_test_name_prefix "${REX_COMPILE_TEST_NAME_PREFIX}")
  endif()
  set(${out_var} "${_test_name_prefix}_${_test_key}" PARENT_SCOPE)
endfunction()

function(rex_register_compile_source_set property_name)
  foreach(source_rel IN LISTS ARGN)
    get_filename_component(_source_abs "${source_rel}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_source_abs}")
      message(FATAL_ERROR
        "CompileTests source '${source_rel}' registered from "
        "${CMAKE_CURRENT_SOURCE_DIR} does not exist")
    endif()
    file(RELATIVE_PATH _source_from_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
         "${_source_abs}")
    set_property(GLOBAL APPEND PROPERTY ${property_name} "${_source_from_root}")
  endforeach()
endfunction()

function(rex_get_registered_compile_source_set out_var property_name)
  get_property(_sources GLOBAL PROPERTY ${property_name})
  if(NOT _sources)
    set(_sources)
  endif()
  list(REMOVE_DUPLICATES _sources)
  list(SORT _sources)
  set(${out_var} "${_sources}" PARENT_SCOPE)
endfunction()

function(rex_verify_compile_test_source_coverage)
  set(options)
  set(oneValueArgs DIRECTORY PATTERN)
  set(multiValueArgs PROPERTIES IGNORE)
  cmake_parse_arguments(REX_VERIFY "${options}" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  if(NOT REX_VERIFY_DIRECTORY)
    set(REX_VERIFY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if(NOT REX_VERIFY_PATTERN)
    set(REX_VERIFY_PATTERN "*.c")
  endif()
  if(NOT REX_VERIFY_PROPERTIES)
    message(FATAL_ERROR "rex_verify_compile_test_source_coverage requires PROPERTIES")
  endif()

  file(GLOB _actual_local_files RELATIVE "${REX_VERIFY_DIRECTORY}"
       "${REX_VERIFY_DIRECTORY}/${REX_VERIFY_PATTERN}")
  file(RELATIVE_PATH _directory_from_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
       "${REX_VERIFY_DIRECTORY}")

  set(_actual_root_relative_files)
  foreach(_file IN LISTS _actual_local_files)
    if(_directory_from_root STREQUAL ".")
      set(_root_relative "${_file}")
    else()
      set(_root_relative "${_directory_from_root}/${_file}")
    endif()
    list(APPEND _actual_root_relative_files "${_root_relative}")
  endforeach()

  set(_registered_root_relative_files)
  foreach(_property IN LISTS REX_VERIFY_PROPERTIES)
    get_property(_registered GLOBAL PROPERTY ${_property})
    if(_registered)
      list(APPEND _registered_root_relative_files ${_registered})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _registered_root_relative_files)

  set(_ignored_root_relative_files)
  foreach(_ignore IN LISTS REX_VERIFY_IGNORE)
    get_filename_component(_ignore_abs "${_ignore}" ABSOLUTE
                           BASE_DIR "${REX_VERIFY_DIRECTORY}")
    if(NOT EXISTS "${_ignore_abs}")
      message(FATAL_ERROR
        "Ignored compile-test source '${_ignore}' does not exist in "
        "${REX_VERIFY_DIRECTORY}")
    endif()
    file(RELATIVE_PATH _ignore_root "${REX_COMPILE_TESTS_SOURCE_DIR}"
         "${_ignore_abs}")
    list(APPEND _ignored_root_relative_files "${_ignore_root}")
  endforeach()
  list(REMOVE_DUPLICATES _ignored_root_relative_files)

  set(_unregistered_files)
  foreach(_file IN LISTS _actual_root_relative_files)
    if(NOT _file IN_LIST _registered_root_relative_files AND
       NOT _file IN_LIST _ignored_root_relative_files)
      list(APPEND _unregistered_files "${_file}")
    endif()
  endforeach()

  set(_stale_registrations)
  foreach(_file IN LISTS _registered_root_relative_files)
    if(_directory_from_root STREQUAL ".")
      set(_belongs_to_directory TRUE)
    else()
      string(FIND "${_file}" "${_directory_from_root}/" _prefix_pos)
      if(_prefix_pos EQUAL 0)
        set(_belongs_to_directory TRUE)
      else()
        set(_belongs_to_directory FALSE)
      endif()
    endif()

    if(_belongs_to_directory AND
       NOT _file IN_LIST _actual_root_relative_files)
      list(APPEND _stale_registrations "${_file}")
    endif()
  endforeach()

  if(_unregistered_files OR _stale_registrations)
    list(SORT _unregistered_files)
    list(SORT _stale_registrations)

    set(_message "CompileTests source coverage mismatch in ${REX_VERIFY_DIRECTORY}")
    if(_unregistered_files)
      string(APPEND _message "\nUnregistered sources:")
      foreach(_file IN LISTS _unregistered_files)
        string(APPEND _message "\n  ${_file}")
      endforeach()
    endif()
    if(_stale_registrations)
      string(APPEND _message "\nStale registrations:")
      foreach(_file IN LISTS _stale_registrations)
        string(APPEND _message "\n  ${_file}")
      endforeach()
    endif()
    message(FATAL_ERROR "${_message}")
  endif()
endfunction()

# determine what subdirectories to enter based on our configuration settings.

add_subdirectory(CudaTests)

# REX: Fortran tests are independent of which C/C++ compiler is used to build REX itself
# Move Fortran test registration outside the Clang-specific if/else block
if(ENABLE-FORTRAN)
  add_subdirectory(Fortran_tests)
  add_subdirectory(experimental_fortran_tests)
endif()

# REX: Enable all tests regardless of compiler (Clang frontend is now standard)
if(ENABLE-C)
  add_subdirectory(C_tests)
  add_subdirectory(C89_std_c89_tests)
  add_subdirectory(C11_tests)
  add_subdirectory(C_subset_of_Cxx_tests)
  add_subdirectory(OpenClTests)
  add_subdirectory(Cxx_tests)
  add_subdirectory(Cxx03_tests)
  add_subdirectory(Cxx11_tests)
  add_subdirectory(Cxx14_tests)
  add_subdirectory(Cxx17_tests)
  add_subdirectory(Cxx20_tests)
  add_subdirectory(C99_tests)
  add_subdirectory(OpenACC_tests)

  add_subdirectory(C++Code)
  add_subdirectory(copyAST_tests)
  # ROSE-1738
  add_subdirectory(RoseExample_tests)
  add_subdirectory(unparseToString_tests)
  add_subdirectory(sourcePosition_tests)
  if(REX_ENABLE_UNINITIALIZED_FIELD_TESTS)
    add_subdirectory(uninitializedField_tests)
  endif()
  add_subdirectory(OvertureCode)
  add_subdirectory(ExpressionTemplateExample_tests)
  add_subdirectory(hiddenTypeAndDeclarationListTests)
  add_subdirectory(sizeofOperation_tests)

  add_subdirectory(nameQualificationAndTypeElaboration_tests)
  add_subdirectory(unparse_template_from_ast)
  add_subdirectory(STL_tests)

  # Legacy frontend interface tests are obsolete
  # if(enable-legacy-frontend-interface)
  #   add_subdirectory(LegacyFrontendInterface_C_tests)
  # endif()

  add_subdirectory(UnparseHeadersTests)
  add_subdirectory(UnparseHeadersUsingTokenStream_tests)
  add_subdirectory(frontend_integration)

  add_subdirectory(mergeAST_tests)
  add_subdirectory(ElsaTestCases)
  add_subdirectory(virtualCFG_tests)
  add_subdirectory(mixLanguage_tests)
  add_subdirectory(systemc_tests)
  add_subdirectory(staticCFG_tests)
endif()

if(ENABLE-C OR ENABLE-FORTRAN)
  add_subdirectory(OpenMP_tests)
  if(EXISTS "${CMAKE_SOURCE_DIR}/tests/external/OpenMP_VV/tests")
    add_subdirectory(OpenMP_VV_tests)
  else()
    message(STATUS "Skipping OpenMP_VV_tests: tests/external/OpenMP_VV/tests not found")
  endif()
endif()

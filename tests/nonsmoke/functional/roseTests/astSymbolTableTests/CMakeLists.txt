#-------------------------------------------------------------------------------
add_executable(testSymbolTable testSymbolTable.C)
target_link_libraries(testSymbolTable
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_template_symbols rex_test2026_template_symbols.C)
target_link_libraries(rex_test2026_template_symbols
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_preprocessing_unparse
  rex_test2026_preprocessing_unparse.C)
target_link_libraries(rex_test2026_preprocessing_unparse
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_openmp_acc_ast
  rex_test2026_openmp_acc_ast.C)
target_link_libraries(rex_test2026_openmp_acc_ast
  ROSE_DLL ${link_with_libraries})

add_executable(rex_test2026_rebuild_symbol_table_coverage
  rex_test2026_rebuild_symbol_table_coverage.cpp)
target_link_libraries(rex_test2026_rebuild_symbol_table_coverage
  ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME astSymbolTable_test1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=testSymbolTable input.C [test1.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testSymbolTable> -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C"
    ${_exit_status}
    test1.passed
)

add_test(
  NAME testSymbolTable_test1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testSymbolTable>
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C
)

add_test(
  NAME rex_test2026_astSymbolTable_template_symbols
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_test2026_template_symbols>
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_template_symbols.C
)

add_test(
  NAME rex_test2026_astSymbolTable_preprocessing_unparse
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_test2026_preprocessing_unparse>
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_preprocessing_unparse.C
)

add_test(
  NAME rex_test2026_astSymbolTable_openmp_acc_ast
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_test2026_openmp_acc_ast>
    -rose:openmp:ast_only
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_openmp_acc.C
)

add_test(
  NAME rex_test2026_astSymbolTable_rebuild_declaration_scope
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_test2026_rebuild_symbol_table_coverage>
    --rex-rebuild-check=declaration-scope
    -c ${CMAKE_CURRENT_SOURCE_DIR}/../astInterfaceTests/inputrex_test2026_declaration_scope.C
)

add_test(
  NAME rex_test2026_astSymbolTable_rebuild_openmp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:rex_test2026_rebuild_symbol_table_coverage>
    --rex-rebuild-check=openmp
    -rose:openmp:ast_only
    -w
    -rose:verbose
    0
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_rebuild_symbol_table_openmp.c
)

if(ENABLE-FORTRAN)
  add_test(
    NAME rex_test2026_astSymbolTable_rebuild_fortran_openmp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:rex_test2026_rebuild_symbol_table_coverage>
      --rex-rebuild-check=fortran-openmp
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -w
      -rose:verbose
      0
      -c ${CMAKE_CURRENT_SOURCE_DIR}/input_rex_test2026_rebuild_symbol_table_fortran.f90
  )
endif()

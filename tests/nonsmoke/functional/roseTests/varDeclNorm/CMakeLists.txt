include_directories(${ROSE_INCLUDES})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_var_decl_conf ${CMAKE_CURRENT_SOURCE_DIR}/varDeclNorm.conf)

set(_rose_include_flags)
foreach(entry IN LISTS ROSE_INCLUDES)
  if(entry MATCHES "^-I" OR entry MATCHES "^-isystem")
    list(APPEND _rose_include_flags ${entry})
  else()
    list(APPEND _rose_include_flags -I${entry})
  endif()
endforeach()

set(_var_decl_specimens
  int_global_scope.C
  int_function_scope.C
  int_struct_scope.C
  mix_global_scope.C
  nonreal_type_template_class_scope.C
)

set(_var_decl_flags
  -rose:suppress_variable_declaration_normalization
  -rose:skipfinalCompileStep
  -w
  -rose:verbose 0)
list(JOIN _var_decl_flags " " _var_decl_flags_str)
list(JOIN _rose_include_flags " " _rose_include_flags_str)

foreach(specimen IN LISTS _var_decl_specimens)
  set(_target ${specimen}.passed)
  add_test(
    NAME varDeclNorm_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="varDeclNorm [${specimen}]"
      EXE="$<TARGET_FILE:testTranslator> ${_var_decl_flags_str} ${_rose_include_flags_str} -I${CMAKE_CURRENT_SOURCE_DIR}"
      INPUT=${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      ${_var_decl_conf}
      ${_target}
  )
endforeach()


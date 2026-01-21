if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  set(general_language_support_sources
    ModuleBuilder.C
    PosInfo.C
    SageTreeBuilder.C
    Tokens.C)

  if(NOT ENABLE-FORTRAN-OFP)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/preproc-fortran.cc
             ${CMAKE_CURRENT_BINARY_DIR}/preproc-fortran-fixed.cc
      COMMAND ${FLEX_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/src/frontend/OpenFortranParser_SAGE_Connection/preproc-fortran.ll
      COMMAND ${CMAKE_COMMAND} -E rename lex.yy.c preproc-fortran.cc
      COMMAND ${FLEX_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/src/frontend/OpenFortranParser_SAGE_Connection/preproc-fortran-fixed.ll
      COMMAND ${CMAKE_COMMAND} -E rename lex.yy.c preproc-fortran-fixed.cc
      DEPENDS ${CMAKE_SOURCE_DIR}/src/frontend/OpenFortranParser_SAGE_Connection/preproc-fortran.ll
              ${CMAKE_SOURCE_DIR}/src/frontend/OpenFortranParser_SAGE_Connection/preproc-fortran-fixed.ll)

    list(APPEND general_language_support_sources
      ${CMAKE_CURRENT_BINARY_DIR}/preproc-fortran.cc
      ${CMAKE_CURRENT_BINARY_DIR}/preproc-fortran-fixed.cc)
  endif()

  add_library(general_language_support OBJECT
    ${general_language_support_sources})

  target_include_directories(general_language_support PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/frontend/OpenFortranParser_SAGE_Connection
    ${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

  add_dependencies(general_language_support rosetta_generated)
  target_compile_features(general_language_support PRIVATE cxx_std_17)
endif()

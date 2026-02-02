if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  set(general_language_support_sources
    ModuleBuilder.C
    PosInfo.C
    SageTreeBuilder.C
    Tokens.C)

  add_library(general_language_support OBJECT
    ${general_language_support_sources})

  target_include_directories(general_language_support PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

  add_dependencies(general_language_support rosetta_generated)
  target_compile_features(general_language_support PRIVATE cxx_std_17)
endif()

if(enable-fortran AND enable-fortran-flang)
  add_library(general_language_support OBJECT
    ModuleBuilder.C
    PosInfo.C
    SageTreeBuilder.C
    Tokens.C)

  target_include_directories(general_language_support PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

  target_compile_features(general_language_support PRIVATE cxx_std_17)
endif()

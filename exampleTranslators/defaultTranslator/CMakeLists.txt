foreach(translator_src IN ITEMS
  defaultTranslator.C
  preprocessingInfoDumper.C
  mangledNameDumper.C
  qualifiedNameDumper.C
  textASTGenerator.C
  fileInfoDumper.C
  simpleTraversal.C
  astTermGenerator.C)
  get_filename_component(translator_name "${translator_src}" NAME_WE)
  add_executable(${translator_name} ${translator_src})
  target_link_libraries(${translator_name} PRIVATE ROSE_DLL)
  install(TARGETS ${translator_name} DESTINATION bin)
endforeach()

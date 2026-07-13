set(DEFAULT_TRANSLATOR_SOURCES
  defaultTranslator.C
  preprocessingInfoDumper.C
  mangledNameDumper.C
  qualifiedNameDumper.C
  textASTGenerator.C
  fileInfoDumper.C
  simpleTraversal.C
  astTermGenerator.C)

set(DEFAULT_TRANSLATOR_TARGETS "")
foreach(translator_src IN LISTS DEFAULT_TRANSLATOR_SOURCES)
  get_filename_component(translator_name "${translator_src}" NAME_WE)
  add_executable(${translator_name} ${translator_src})
  add_dependencies(${translator_name} ROSE_DLL)
  target_link_libraries(${translator_name} PRIVATE ROSE_DLL)
  install(TARGETS ${translator_name} DESTINATION bin)
  list(APPEND DEFAULT_TRANSLATOR_TARGETS ${translator_name})
endforeach()

if(TARGET rose_clang_libstdcxx_headers)
  foreach(translator IN LISTS DEFAULT_TRANSLATOR_TARGETS)
    target_link_libraries(${translator} PRIVATE rose_clang_libstdcxx_headers)
  endforeach()
endif()

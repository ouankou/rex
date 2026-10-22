set(OUTLINER_SOURCES
  outlineTutorial.cc
  outlineIfs.cc
  outlineIfs2.C
  outlinePreproc.cc)

foreach(outliner_src IN LISTS OUTLINER_SOURCES)
  get_filename_component(outliner_name "${outliner_src}" NAME_WE)
  add_executable(${outliner_name} ${outliner_src})
  target_link_libraries(${outliner_name} ROSE_DLL)
endforeach()

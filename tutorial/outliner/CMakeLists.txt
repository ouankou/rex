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

# Install only those executables that are useful to users after ROSE is installed. If the main purpose of compiling
# the tool is to ensure that the source code is correct for the purposes of documentation in the various tutorials,
# then there's no point in installing that executable.
#install(TARGETS outline DESTINATION bin)

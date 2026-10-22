if(ENABLE-FORTRAN)
  add_executable(
    tools_checkFortranInterfaces_exe
    checkFortranInterfaces.h checkFortranInterfaces.C main.C)

  target_link_libraries(tools_checkFortranInterfaces_exe PRIVATE ROSE_DLL)

  target_include_directories(
    tools_checkFortranInterfaces_exe
    PRIVATE ${ROSE_INCLUDES})
  set_target_properties(tools_checkFortranInterfaces_exe
                        PROPERTIES OUTPUT_NAME "checkFortranInterfaces")

  install(TARGETS tools_checkFortranInterfaces_exe DESTINATION bin)

  add_dependencies(tools tools_checkFortranInterfaces_exe)
endif()

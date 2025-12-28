if(NOT enable-internalFrontendDevelopment)
  # These jars are internal; do not install into ${ROSE_LIB_DIR_NAME}.
  #install(FILES antlr-3.5.2-complete.jar DESTINATION ${ROSE_LIB_DIR_NAME})
endif()

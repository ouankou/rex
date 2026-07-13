include_directories( ${CMAKE_SOURCE_DIR}/src/frontend/Flang/builder )

add_library(sageInterface OBJECT
  edgePointerReplacement.C
  sageInterface.C
  sageInterface_type.C
  nonrealQualificationSupport.C
  generateUniqueName.C
  sageBuilder.C
  sageBuilder_fortran.C
  abiStuff.C)

add_dependencies(sageInterface rosetta_generated)

rose_force_system_include_dirs(sageInterface ${LLVM_INCLUDE_DIRS})

install(
  FILES  edgePointerReplacement.h sageInterface.h sageBuilder.h abiStuff.h
         sageFunctors.h sageGeneric.h
  DESTINATION ${INCLUDE_INSTALL_DIR}
  )

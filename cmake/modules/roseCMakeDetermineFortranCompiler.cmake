
# determine the compiler to use for Fortran programs
# NOTE, a generator may set CMAKE_Fortran_COMPILER before
# loading this file to force a compiler.
# use environment variable FC first if defined by user, next use 
# the cmake variable CMAKE_GENERATOR_FC which can be defined by a generator
# as a default compiler

IF(NOT CMAKE_Fortran_COMPILER)
  # prefer the environment variable CC
  IF($ENV{FC} MATCHES ".+")
    GET_FILENAME_COMPONENT(CMAKE_Fortran_COMPILER_INIT $ENV{FC} PROGRAM PROGRAM_ARGS CMAKE_Fortran_FLAGS_ENV_INIT)
    IF(CMAKE_Fortran_FLAGS_ENV_INIT)
      SET(CMAKE_Fortran_COMPILER_ARG1 "${CMAKE_Fortran_FLAGS_ENV_INIT}" CACHE STRING "First argument to Fortran compiler")
    ENDIF(CMAKE_Fortran_FLAGS_ENV_INIT)
    IF(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
    ELSE(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
      MESSAGE(FATAL_ERROR "Could not find compiler set in environment variable FC:\n$ENV{FC}.") 
    ENDIF(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
  ENDIF($ENV{FC} MATCHES ".+")
  
  # next try prefer the compiler specified by the generator
  IF(CMAKE_GENERATOR_FC) 
    IF(NOT CMAKE_Fortran_COMPILER_INIT)
      SET(CMAKE_Fortran_COMPILER_INIT ${CMAKE_GENERATOR_FC})
    ENDIF(NOT CMAKE_Fortran_COMPILER_INIT)
  ENDIF(CMAKE_GENERATOR_FC)

  # finally list compilers to try
  IF(CMAKE_Fortran_COMPILER_INIT)
    SET(CMAKE_Fortran_COMPILER_LIST ${CMAKE_Fortran_COMPILER_INIT})
  ELSE(CMAKE_Fortran_COMPILER_INIT)
    # Known compilers:
    #  flang: LLVM Flang (required)
    # NOTE for testing purposes this list is DUPLICATED in
    # CMake/Source/CMakeLists.txt, IF YOU CHANGE THIS LIST,
    # PLEASE UPDATE THAT FILE AS WELL!
    SET(CMAKE_Fortran_COMPILER_LIST
      flang
      )
  ENDIF(CMAKE_Fortran_COMPILER_INIT)

  # Find the compiler.
  FIND_PROGRAM(CMAKE_Fortran_COMPILER NAMES ${CMAKE_Fortran_COMPILER_LIST} DOC "Fortran compiler")
  IF(CMAKE_Fortran_COMPILER_INIT AND NOT CMAKE_Fortran_COMPILER)
    SET(CMAKE_Fortran_COMPILER "${CMAKE_Fortran_COMPILER_INIT}" CACHE FILEPATH "Fortran compiler" FORCE)
  ENDIF(CMAKE_Fortran_COMPILER_INIT AND NOT CMAKE_Fortran_COMPILER)
ELSE(NOT CMAKE_Fortran_COMPILER)
   # we only get here if CMAKE_Fortran_COMPILER was specified using -D or a pre-made CMakeCache.txt
  # (e.g. via ctest) or set in CMAKE_TOOLCHAIN_FILE
  # if CMAKE_Fortran_COMPILER is a list of length 2, use the first item as 
  # CMAKE_Fortran_COMPILER and the 2nd one as CMAKE_Fortran_COMPILER_ARG1

  LIST(LENGTH CMAKE_Fortran_COMPILER _CMAKE_Fortran_COMPILER_LIST_LENGTH)
  IF("${_CMAKE_Fortran_COMPILER_LIST_LENGTH}" EQUAL 2)
    LIST(GET CMAKE_Fortran_COMPILER 1 CMAKE_Fortran_COMPILER_ARG1)
    LIST(GET CMAKE_Fortran_COMPILER 0 CMAKE_Fortran_COMPILER)
  ENDIF("${_CMAKE_Fortran_COMPILER_LIST_LENGTH}" EQUAL 2)

  # if a compiler was specified by the user but without path, 
  # now try to find it with the full path
  # if it is found, force it into the cache, 
  # if not, don't overwrite the setting (which was given by the user) with "NOTFOUND"
  # if the C compiler already had a path, reuse it for searching the CXX compiler
  GET_FILENAME_COMPONENT(_CMAKE_USER_Fortran_COMPILER_PATH "${CMAKE_Fortran_COMPILER}" PATH)
  IF(NOT _CMAKE_USER_Fortran_COMPILER_PATH)
    FIND_PROGRAM(CMAKE_Fortran_COMPILER_WITH_PATH NAMES ${CMAKE_Fortran_COMPILER})
    MARK_AS_ADVANCED(CMAKE_Fortran_COMPILER_WITH_PATH)
    IF(CMAKE_Fortran_COMPILER_WITH_PATH)
      SET(CMAKE_Fortran_COMPILER ${CMAKE_Fortran_COMPILER_WITH_PATH}
        CACHE STRING "Fortran compiler" FORCE)
    ENDIF(CMAKE_Fortran_COMPILER_WITH_PATH)
  ENDIF(NOT _CMAKE_USER_Fortran_COMPILER_PATH)
ENDIF(NOT CMAKE_Fortran_COMPILER)

MARK_AS_ADVANCED(CMAKE_Fortran_COMPILER)  

IF(NOT CMAKE_Fortran_COMPILER_ID_RUN)
  SET(CMAKE_Fortran_COMPILER_ID_RUN 1)

  # Each entry in this list is a set of extra flags to try
  # adding to the compile line to see if it helps produce
  # a valid identification executable.
  SET(CMAKE_Fortran_COMPILER_ID_TEST_FLAGS
    # Try compiling to an object file only.
    "-c"

    # Some Intel compilers do not preprocess by default.
    "-fpp"
    )

  # Try to identify the compiler.
  SET(CMAKE_Fortran_COMPILER_ID)
  INCLUDE(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerId.cmake)
  CMAKE_DETERMINE_COMPILER_ID(Fortran FFLAGS CMakeFortranCompilerId.F90)

  # Fall back to old is-GNU test.
  IF(NOT CMAKE_Fortran_COMPILER_ID)
    EXEC_PROGRAM(${CMAKE_Fortran_COMPILER}
      ARGS ${CMAKE_Fortran_COMPILER_ID_FLAGS_LIST} -E "\"${CMAKE_ROOT}/Modules/CMakeTestGNU.c\""
      OUTPUT_VARIABLE CMAKE_COMPILER_OUTPUT RETURN_VALUE CMAKE_COMPILER_RETURN)
    IF(NOT CMAKE_COMPILER_RETURN)
      IF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
        SET(CMAKE_Fortran_COMPILER_ID "GNU")
        FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
          "Determining if the Fortran compiler is GNU succeeded with "
          "the following output:\n${CMAKE_COMPILER_OUTPUT}\n\n")
      ELSE("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
        FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
          "Determining if the Fortran compiler is GNU failed with "
          "the following output:\n${CMAKE_COMPILER_OUTPUT}\n\n")
      ENDIF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
    ENDIF(NOT CMAKE_COMPILER_RETURN)
  ENDIF(NOT CMAKE_Fortran_COMPILER_ID)

  # Set old compiler and platform id variables.
  IF("${CMAKE_Fortran_COMPILER_ID}" MATCHES "GNU")
    SET(CMAKE_COMPILER_IS_GNUG77 1)
  ENDIF("${CMAKE_Fortran_COMPILER_ID}" MATCHES "GNU")
ENDIF(NOT CMAKE_Fortran_COMPILER_ID_RUN)

INCLUDE(CMakeFindBinUtils)

# configure variables set in this file for fast reload later on
CONFIGURE_FILE(${CMAKE_ROOT}/Modules/CMakeFortranCompiler.cmake.in
  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeFortranCompiler.cmake
  @ONLY IMMEDIATE # IMMEDIATE must be here for compatibility mode <= 2.0
  )
SET(CMAKE_Fortran_COMPILER_ENV_VAR "FC")

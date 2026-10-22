# Read only the package's version metadata. Loading native LLVMConfig.cmake
# would import host targets into a cross-compilation configuration.
function(rose_read_llvm_package_version llvm_root output)
  set(package_dir "${llvm_root}/lib/cmake/llvm")
  foreach(metadata LLVMConfig.cmake LLVMConfigVersion.cmake)
    if(NOT EXISTS "${package_dir}/${metadata}" OR
       IS_DIRECTORY "${package_dir}/${metadata}")
      message(FATAL_ERROR "LLVM package is missing ${package_dir}/${metadata}")
    endif()
  endforeach()
  set(PACKAGE_VERSION "")
  include("${package_dir}/LLVMConfigVersion.cmake")
  if(NOT PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+[A-Za-z0-9.+-]*$")
    message(FATAL_ERROR
      "LLVMConfigVersion.cmake did not provide a valid PACKAGE_VERSION: "
      "${package_dir}/LLVMConfigVersion.cmake")
  endif()
  set(${output} "${PACKAGE_VERSION}" PARENT_SCOPE)
endfunction()

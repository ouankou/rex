/**
 * Copyright (c) 2005, 2006 Los Alamos National Security, LLC.  This
 * material was produced under U.S. Government contract DE-
 * AC52-06NA25396 for Los Alamos National Laboratory (LANL), which is
 * operated by the Los Alamos National Security, LLC (LANS) for the
 * U.S. Department of Energy. The U.S. Government has rights to use,
 * reproduce, and distribute this software. NEITHER THE GOVERNMENT NOR
 * LANS MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
 * LIABILITY FOR THE USE OF THIS SOFTWARE. If software is modified to
 * produce derivative works, such modified software should be clearly
 * marked, so as not to confuse it with the version available from
 * LANL.
 *
 * Additionally, this program and the accompanying materials are made
 * available under the terms of the EPL v1.0 which accompanies this
 * distribution.
 */

/* Based on examples/docs from:
 *      http://java.sun.com/j2se/1.4.2/docs/guide/jni/spec/invocation.html#wp9502
 * http://java.sun.com/j2se/1.4.2/docs/guide/jni/spec/jniTOC.html
 * http://java.sun.com/docs/books/jni/html/invoke.html
 */
#include "sage3basic.h"

#include "assert.h"
#include "commandline_processing.h"
#include "fortran_error_handler.h"
#include "jserver.h"
#include "ofp.h"
#include "rose_config.h"
#include "rose_paths.h"
#include <filesystem>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

/* This is defined if ROSE is configured to use the JVM-based Open Fortran
 * Parser. */
#ifndef USE_ROSE_OPEN_FORTRAN_PARSER_SUPPORT
#error                                                                         \
    "openFortranParser_main.C should not be compiled when OFP support is disabled"
#endif

using namespace std;

namespace fs = std::filesystem;

// DQ (9/6/2010): Allow this to be commented out to simplify debugging using
// gdb. #define ENABLE_FORTRAN_ERROR_HANDLER

/* This is critical to permitting the correct library (the one just built by the
user)
// to be used instead of requiring the LD_LIBRARY_PATH to beexplicitly set by
the user
// before running any tests.  It is alos important to proper testing using "make
distcheck"
// since we want the new library built using that rule to be tested over any
other.
*/
#define OVERWRITE_LD_LIBRARY_PATH 1

int openFortranParser_main(int argc, char **argv) {
  /* To use different versions of the LD_LIBRARY_PATH, get the value,
     change it to the path in the build tree, and then reset it to the
     old value after the call to JVM.  We need to do this because the
     libparser_java_FortranParserActionJNI.so that we need is in the
     build tree and it is specific to the configuration of ROSE (else
     V_<class name> enum values will be different as a result of
     configuration options that might trigger different numbers of IR
     nodes to be use.
   */

  /* Overwite to a new value. It is not clear when to use the install path and
   * when to use the build path! */
#ifdef USE_CMAKE
  string install_lib_dir = findRoseSupportPathFromBuild("lib", "lib");
#else
  string install_lib_dir = findRoseSupportPathFromBuild(
      "src/frontend/OpenFortranParser_SAGE_Connection/.libs", "lib");
#endif

  /* Save the old value */
  const char *old_value = getenv(ROSE_SHLIBPATH_VAR); // Might be null
  std::string old_value_str = old_value ? old_value : "";

#if OVERWRITE_LD_LIBRARY_PATH
  std::vector<std::string> runtime_paths;
  std::set<std::string> seen_paths;

  auto add_runtime_dir = [&](const fs::path &dir, bool require_jvm) {
    if (dir.empty())
      return;
    std::error_code ec;
    fs::path normalized_dir = dir;
    if (require_jvm) {
      fs::path lib_candidate = dir / "libjvm.so";
      if (!fs::exists(lib_candidate, ec))
        return;
    }
    std::string normalized = fs::weakly_canonical(dir, ec).string();
    if (ec || normalized.empty())
      normalized = dir.lexically_normal().string();
    if (normalized.empty())
      return;
    if (seen_paths.insert(normalized).second) {
      runtime_paths.push_back(normalized);
    }
  };

  add_runtime_dir(install_lib_dir, /*require_jvm=*/false);

  auto record_java_home_variants = [&](const fs::path &java_home) {
    if (java_home.empty())
      return;
    add_runtime_dir(java_home / "lib/server", /*require_jvm=*/true);
    add_runtime_dir(java_home / "lib/amd64/server", /*require_jvm=*/true);
    add_runtime_dir(java_home / "lib/jli", /*require_jvm=*/true);
    add_runtime_dir(java_home / "lib", /*require_jvm=*/true);
  };

#ifdef OFP_JVM_PATH
  {
    fs::path java_exe = fs::path(OFP_JVM_PATH);
    fs::path java_bin = java_exe.parent_path();
    fs::path java_home = java_bin.parent_path();
    record_java_home_variants(java_home);
  }
#endif

  if (const char *java_home_env = getenv("JAVA_HOME")) {
    record_java_home_variants(fs::path(java_home_env));
  }
  if (const char *jre_home_env = getenv("JRE_HOME")) {
    record_java_home_variants(fs::path(jre_home_env));
  }

  if (!old_value_str.empty()) {
    std::stringstream ss(old_value_str);
    std::string segment;
    while (std::getline(ss, segment, ':')) {
      if (!segment.empty())
        add_runtime_dir(segment, /*require_jvm=*/false);
    }
  }

  std::string combined_ld_path;
  for (size_t i = 0; i < runtime_paths.size(); ++i) {
    if (i != 0)
      combined_ld_path += ":";
    combined_ld_path += runtime_paths[i];
  }

  int overwrite = 1;
  int env_status =
      setenv(ROSE_SHLIBPATH_VAR, combined_ld_path.c_str(), overwrite);
  assert(env_status == 0);
#endif

  if (SgProject::get_verbose() > 0) {
    printf("Call the function that will start a JVM and call the OFP \n\n");
    string JVM_command_line = CommandlineProcessing::generateStringFromArgList(
        CommandlineProcessing::generateArgListFromArgcArgv(argc, argv));
    printf("Java JVM commandline = %s \n", JVM_command_line.c_str());
    printf("ROSE modified %s = %s \n", ROSE_SHLIBPATH_VAR,
           combined_ld_path.c_str());
  }

#ifdef ENABLE_FORTRAN_ERROR_HANDLER
  fortran_error_handler_begin();
#endif

  int status = Rose::Frontend::Fortran::Ofp::jvm_ofp_processing(argc, argv);

#ifdef ENABLE_FORTRAN_ERROR_HANDLER
  fortran_error_handler_end();
#endif

  if (SgProject::get_verbose() > 0) {
    printf("JVM processing done.\n\n");
  }

  Rose::Frontend::Fortran::Ofp::jserver_finish();

  /* Reset to the saved value */
#if OVERWRITE_LD_LIBRARY_PATH
  // DQ (9/12/2011): Note that old_value can be NULL and if so then we don't
  // want it to be dereferenced. env_status =
  // setenv(ROSE_SHLIBPATH_VAR,old_value,overwrite);
  if (!old_value_str.empty())
    env_status = setenv(ROSE_SHLIBPATH_VAR, old_value_str.c_str(), 1);
  else
    env_status = unsetenv(ROSE_SHLIBPATH_VAR);

  assert(env_status == 0);
#endif

  return status;
}

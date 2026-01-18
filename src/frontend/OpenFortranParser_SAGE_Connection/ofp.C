#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "jserver.h"
#include "ofp.h"

namespace Rose {
namespace Frontend {
namespace Fortran {
namespace Ofp {

using std::string;

static jclass jofp_get_class(const char *name);
static jmethodID jofp_get_method(jclass xclass, int static_method,
                                 const char *name, const char *arg);
static jclass jofp_get_frontEnd_class();
static jmethodID jofp_get_frontEnd_main_method(jclass front_end_class);
static jmethodID jofp_get_frontEnd_getError_method(jclass front_end_class);

int jvm_ofp_processing(int argc, char **argv) {
  JNIEnv *env = getEnv();
  jobjectArray args = jserver_getJavaStringArray(argc, argv);
  if (args == NULL)
    jserver_handleException();

  jclass front_end_class = jofp_get_frontEnd_class();
  jmethodID main_method = jofp_get_frontEnd_main_method(front_end_class);
  jmethodID get_error_method =
      jofp_get_frontEnd_getError_method(front_end_class);

  jserver_callMethod(front_end_class, main_method, args);
  int retval =
      jserver_callStaticBooleanMethod(front_end_class, get_error_method);

  env->DeleteLocalRef(args);
  env->DeleteLocalRef(front_end_class);

  return retval;
}

static jclass jofp_get_class(const char *name) {
  jclass xclass = jserver_FindClass(name);
  if (xclass == NULL)
    jserver_handleException();
  return xclass;
}

static jmethodID jofp_get_method(jclass xclass, int static_method,
                                 const char *name, const char *arg) {
  jmethodID method = jserver_GetMethodID(static_method, xclass, name, arg);
  if (method == NULL)
    jserver_handleException();
  return method;
}

static jclass jofp_get_frontEnd_class() {
  return jofp_get_class("fortran/ofp/FrontEnd");
}

static jmethodID jofp_get_frontEnd_main_method(jclass front_end_class) {
  return jofp_get_method(front_end_class, STATIC_METHOD, "main",
                         "([Ljava/lang/String;)V");
}

static jmethodID jofp_get_frontEnd_getError_method(jclass front_end_class) {
  return jofp_get_method(front_end_class, STATIC_METHOD, "getError", "()Z");
}

} // namespace Ofp
} // namespace Fortran
} // namespace Frontend
} // namespace Rose

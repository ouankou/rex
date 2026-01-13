/* Based on examples/docs from:
 * http://java.sun.com/j2se/1.4.2/docs/guide/jni/spec/invocation.html#wp9502
 * http://java.sun.com/j2se/1.4.2/docs/guide/jni/spec/jniTOC.html
 * http://java.sun.com/docs/books/jni/html/invoke.html
 */
#include "sage3basic.h"

#include <string.h>
#include <string>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <filesystem>
#include <dlfcn.h>

#include "cmdline.h"
#include "commandline_processing.h"

#include "jserver.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"

using namespace std;

namespace Rose {
namespace Frontend {
namespace Fortran {
namespace Ofp {

typedef struct {
   JavaVM * jvm;
   JNIEnv * env;
} JvmT;

static JvmT je;
namespace {
using CreateJavaVmFn = jint (*)(JavaVM **, void **, void *);

struct JvmLibrary {
  void* handle = nullptr;
  CreateJavaVmFn create_vm = nullptr;
};

JvmLibrary& getJvmLibrary() {
  static JvmLibrary library;
  return library;
}

void* tryOpenJvmLibrary(const char* path) {
  dlerror();
  return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
}

std::vector<std::string> buildJvmCandidatePaths() {
  namespace fs = std::filesystem;
  std::vector<std::string> candidates;

  auto add_dir = [&](const fs::path& dir) {
    if (dir.empty()) {
      return;
    }
    candidates.push_back((dir / "libjvm.so").string());
  };

  auto add_java_home = [&](const fs::path& java_home) {
    if (java_home.empty()) {
      return;
    }
    add_dir(java_home / "lib/server");
    add_dir(java_home / "lib/amd64/server");
    add_dir(java_home / "lib/jli");
    add_dir(java_home / "lib");
  };

  std::string ofp_path = OFP_JVM_PATH;
  if (!ofp_path.empty()) {
    fs::path java_exe(ofp_path);
    fs::path java_home = java_exe.parent_path().parent_path();
    add_java_home(java_home);
  }

  if (const char* java_home_env = getenv("JAVA_HOME")) {
    add_java_home(fs::path(java_home_env));
  }
  if (const char* jre_home_env = getenv("JRE_HOME")) {
    add_java_home(fs::path(jre_home_env));
  }

  return candidates;
}

CreateJavaVmFn loadCreateJavaVm() {
  JvmLibrary& lib = getJvmLibrary();
  if (lib.create_vm != nullptr) {
    return lib.create_vm;
  }

  lib.handle = tryOpenJvmLibrary("libjvm.so");
  std::vector<std::string> candidates;
  if (lib.handle == nullptr) {
    candidates = buildJvmCandidatePaths();
    for (const auto& candidate : candidates) {
      lib.handle = tryOpenJvmLibrary(candidate.c_str());
      if (lib.handle != nullptr) {
        break;
      }
    }
  }

  if (lib.handle == nullptr) {
    const char* error = dlerror();
    fprintf(stderr, "Failed to load libjvm.so");
    if (!candidates.empty()) {
      fprintf(stderr, ". Tried:\n");
      for (const auto& candidate : candidates) {
        fprintf(stderr, "  %s\n", candidate.c_str());
      }
    }
    if (error != nullptr) {
      fprintf(stderr, "dlopen error: %s\n", error);
    } else {
      fprintf(stderr, "\n");
    }
    ROSE_ABORT();
  }

  dlerror();
  void* symbol = dlsym(lib.handle, "JNI_CreateJavaVM");
  const char* error = dlerror();
  if (symbol == nullptr || error != nullptr) {
    fprintf(stderr, "Failed to resolve JNI_CreateJavaVM: %s\n",
            error ? error : "unknown error");
    ROSE_ABORT();
  }

  lib.create_vm = reinterpret_cast<CreateJavaVmFn>(symbol);
  return lib.create_vm;
}
} // namespace

//Warning ! do not make these static as gdb cannot stop at a static function
JNIEnv*  get_env();
JavaVM*  get_jvm();
JvmT*    get_jvmEnv();
void     jserver_start(JvmT* je);
void     jserver_destroy();
jclass   jserver_getJavaStringClass();

/* 
 * This function does nothing since Java VM will
 * be loaded and started automatically when it is needed.
 *
 */
// Rasmussen (2/17/2019): This function should not be defined. For some very strange reason
// having it defined and not optimized away (I think) doesn't actually allow use of the JNI
// functions defined in this file. I believe that JNI_CreateJavaVM in jserver_start effectively
// initializes the JVM.
//
// void jserver_init() { return ; }

JNIEnv* getEnv() {
   return get_env();
}

void 
jserver_finish()
{
   return jserver_destroy();
}

jclass 
jserver_FindClass(const char *name)
{
   JNIEnv *env = get_env();
   return env->FindClass(name);
};


jmethodID
jserver_GetMethodID(int static_method, jclass obj_class, 
                           const char* name, const char* path)
{
   JNIEnv *env = get_env();
   if (static_method==STATIC_METHOD)   
     return  env->GetStaticMethodID(obj_class,name, path);
   else
     return  env->GetMethodID(obj_class,name, path);
};

void
jserver_callMethod(jclass obj_class, jmethodID method, jobjectArray args)
{  
    JNIEnv *env = get_env();
    env->CallStaticVoidMethod(obj_class, method, args);
    return ;
};

int
jserver_callStaticBooleanMethod(jclass obj_class, jmethodID method)
{
    int retv = 0; 
    JNIEnv *env = get_env();

    retv = env->CallStaticBooleanMethod(obj_class, method);
    return retv;
};


//! Java VM loaded here
/*! This function return <jvm, env> if Java VM is already load/run
 *  otherwise load/run Java VM
 */
JvmT* 
get_jvmEnv()
{
  if (je.jvm == NULL ) {
       jserver_start(&je);
  }
  return &je; 
}

JavaVM* 
get_jvm()
{
   return (get_jvmEnv())->jvm;
}

JNIEnv* 
get_env()
{
   return (get_jvmEnv())->env;
}

void 
jserver_start(JvmT* je)
{
  JavaVMInitArgs jvm_args;  /* VM initialization args.  */
  // Rasmussen (6/28/2017): Bumped to latest(?), unsure why 1_4 was used
  // Rasmussen (2/13/2019): Increased JNI version to 1.8.  This should
  // remove need to install legacy jdk version 1.6.
  jvm_args.version = JNI_VERSION_1_8;
  jvm_args.ignoreUnrecognized = JNI_FALSE;

  //----------------------------------------------------------------------------
  // Add all our JVM options
  //----------------------------------------------------------------------------
  // TOO1 (2/11/2014): JVM options now stored in the Cmdline::Fortran::Ofp namespace.
  std::list<std::string> jvm_options =
      Rose::Cmdline::Fortran::Ofp::jvm_options;

  std::string classpath =
      Rose::Cmdline::Fortran::Ofp::GetRoseClasspath();
  jvm_options.push_back(classpath);

  // Rasmussen (6/28/2017): Increasing stacksize fixes crashes on some rhel7 systems
  std::string stack_option = "-Xss2m";
  jvm_options.push_back(stack_option);

  std::vector<std::string> option_storage;
  option_storage.reserve(jvm_options.size());
  for (const auto &option : jvm_options) {
    option_storage.push_back(option);
  }

  std::vector<JavaVMOption> options;
  options.reserve(option_storage.size());
  for (auto &option : option_storage) {
    JavaVMOption jvm_option;
    jvm_option.optionString = option.data();
    jvm_option.extraInfo = nullptr;
    options.push_back(jvm_option);
  }

  jvm_args.nOptions = static_cast<jint>(options.size());
  jvm_args.options = options.data();

  //----------------------------------------------------------------------------
  // Create and load the Java VM.
  //----------------------------------------------------------------------------
  CreateJavaVmFn create_vm = loadCreateJavaVm();
  jint res = create_vm(&(je->jvm), (void **)&(je->env), &jvm_args);

  if (res != JNI_OK) {
      printf("Failed to create Java VM\n");
  }

//sanity check
  JNIEnv* env = get_env();
  ROSE_ASSERT(env->GetVersion() >= JNI_VERSION_1_8);

  if (res<0 || je->jvm==NULL || je->env==NULL)
  {
      exit(1);
  }

  return ;
}

void 
jserver_destroy()
{
    if( je.jvm != NULL  ){
         je.jvm->DestroyJavaVM();
    }
}

void 
jserver_handleException() 
{
    JNIEnv* env = get_env();
     if(env->ExceptionOccurred())
        env->ExceptionDescribe();

     jserver_destroy();

     /* Exit since the exception should mean we can't recover.  */
     exit(1);
}

jobject
jserver_getNewObject(jclass cls,jmethodID method, jobjectArray args,
                                        jstring name,jstring type)
{
    JNIEnv* env = get_env();
    return env->NewObject(cls,method, args, name, type);

}
  
jstring 
jserver_getJavaString(const char *inString)
{
         JNIEnv*  env = get_env();
         return env->NewStringUTF(inString);
}

jobjectArray 
jserver_getJavaStringArray(int argc, char **argv)
{
         jobjectArray argsStringArray = NULL;
         jclass stringClass;
         int i;
         JNIEnv * env = get_env();

         /* We need the String class because that is the underlying type of
                 the array.  */
         stringClass = jserver_getJavaStringClass();
         if(stringClass == NULL)
                return NULL;

         /* Build a new object array.  Params are: env, length, class type of the 
                 array, initial object(?).  */
         argsStringArray = env->NewObjectArray((argc-1), stringClass, NULL);

         if(argsStringArray == NULL)
                return NULL;

         /* Put all args from argv, after the first (which is this program's
                 name) into the array of Strings for FortranMain.  The args array
                 for Java does not include the program name.  */
         for(i = 1; i < argc; i++)
           env->SetObjectArrayElement(argsStringArray, (jsize)i-1,
                                                jserver_getJavaString(argv[i]));

         return argsStringArray;
}

jclass 
jserver_getJavaStringClass()
{
      JNIEnv * env = get_env();
       return env->FindClass("java/lang/String");
}

}// ::Rose::Frontend::Fortran::Ofp
}// ::Rose::Frontend::Fortran
}// ::Rose::Frontend
} // namespace Rose


// DQ (9/28/2017): Removed this to avoid dependence on ROSETTA generated code.
// DQ (9/8/2017): Added header file suppport to allow for SgProject to be referenced for debugging.
#include "rosePublicConfig.h"
// #include "rose_config.h"
// #include "sage3basic.h"

#include "processSupport.h"

#include <cassert>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include "rosedll.h"

using namespace std;

int systemFromVector(const vector<string>& argv)
   {
  assert(!argv.empty());
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    abort();
  }

     if (pid == 0)
        { // Child
          vector<const char*> argvC(argv.size() + 1);
          size_t length = argv.size();

       // DQ (6/22/2020): Truncate as a test!
       // length = 5;

       // for (size_t i = 0; i < argv.size(); ++i)
          for (size_t i = 0; i < length; ++i)
             {
               argvC[i] = strdup(argv[i].c_str());
#if 0
               printf ("In systemFromVector(): loop: argvC[%zu] = %s \n",i,argvC[i]);
#endif
             }
          argvC.back() = NULL;
          execvp(argv[0].c_str(), (char* const*)&argvC[0]);

#if 0
       // DQ (9/12/2017): This is one approach to debugging this (which was required for Ada because the Ada compiler is a call to gnat with the extra option "compile".
       // execvp(argv[0].c_str(), (char* const*)&argvC[0]);
       // execvp("/home/quinlan1/ROSE/ADA/x86_64-linux/adagpl-2017/gnatgpl/gnat-gpl-2017-x86_64-linux-bin/bin/gnat compile",(char* const*)&argvC[0]);
          execvp("/home/quinlan1/ROSE/ADA/x86_64-linux/adagpl-2017/gnatgpl/gnat-gpl-2017-x86_64-linux-bin/bin/gnat",(char* const*)&argvC[0]);
#endif

          perror(("execvp in systemFromVector: " + argv[0]).c_str());
          exit(1); // Should not get here normally
        }
       else
        { // Parent
          int status;
          pid_t err = waitpid(pid, &status, 0);
#if 0
          printf ("In systemFromVector(): status = %d \n",status);
#endif
          if (err == -1) {perror("waitpid"); abort();}

          return status;
        }
   }

// EOF is not handled correctly here -- EOF is normally set when the child
// process exits
FILE* popenReadFromVector(const vector<string>& argv) {
  assert (!argv.empty());
  int pipeDescriptors[2];

  int pipeErr = pipe(pipeDescriptors);
  if (pipeErr == -1) {perror("pipe"); abort();}
  pid_t pid = fork();
  if (pid == -1) {perror("fork"); abort();}
  if (pid == 0) { // Child
    vector<const char*> argvC(argv.size() + 1);
    for (size_t i = 0; i < argv.size(); ++i) {
      argvC[i] = strdup(argv[i].c_str());
    }
    argvC.back() = NULL;
    int closeErr = close(pipeDescriptors[0]);
    if (closeErr == -1) {perror("close (in child)"); abort();}
    int dup2Err = dup2(pipeDescriptors[1], 1); // stdout
    if (dup2Err == -1) {perror("dup2"); abort();}
    execvp(argv[0].c_str(), (char* const*)&argvC[0]);
    perror(("execvp in popenReadFromVector: " + argv[0]).c_str());
    exit(1); // Should not get here normally
  } else { // Parent
    int closeErr = close(pipeDescriptors[1]);
    if (closeErr == -1) {perror("close (in parent)"); abort();}
    return fdopen(pipeDescriptors[0], "r");
  }
}

int pcloseFromVector(FILE* f)
   {
  // Assumes there is only one child process
  int status = 0;

  /* pid_t err = */ wait(&status);

  fclose(f);
  return status;
   }

#ifndef ROSE_PROCESSSUPPORT_H
#define ROSE_PROCESSSUPPORT_H

#include "rosedll.h"
#include <string>
#include <vector>

// Executes literal arguments without a shell and waits for that exact child.
// Returns its exit code, 128 + signal on signal termination, or 127 if exec
// fails. Process creation/wait failures are fatal. This is an exit code, not
// the packed POSIX wait status returned by system(3).
ROSE_UTIL_API int systemFromVector(const std::vector<std::string> &argv);

#endif // ROSE_PROCESSSUPPORT_H

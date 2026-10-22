#include "processSupport.h"
#include "rosePublicConfig.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

int systemFromVector(const std::vector<std::string> &argv) {
  if (argv.empty() || argv.front().empty()) {
    std::fputs("systemFromVector requires an executable\n", stderr);
    std::abort();
  }
  // Allocate before fork: the child may inherit locks from other threads.
  std::vector<char *> arguments;
  arguments.reserve(argv.size() + 1);
  for (const std::string &argument : argv) {
    if (argument.find('\0') != std::string::npos) {
      std::fputs("systemFromVector argument contains a NUL byte\n", stderr);
      std::abort();
    }
    arguments.push_back(const_cast<char *>(argument.c_str()));
  }
  arguments.push_back(nullptr);
  const std::string execFailure = "execvp failed: " + argv.front() + "\n";
  const pid_t pid = fork();
  if (pid == -1) {
    std::perror("fork");
    std::abort();
  }
  if (pid == 0) {
    execvp(arguments.front(), arguments.data());
    // Failed exec must not flush inherited streams or run parent cleanup.
    (void)write(STDERR_FILENO, execFailure.data(), execFailure.size());
    _exit(127);
  }

  int status = 0;
  pid_t result;
  do {
    result = waitpid(pid, &status, 0);
  } while (result == -1 && errno == EINTR);
  if (result != pid) {
    std::perror("waitpid");
    std::abort();
  }
  // Returning the packed wait status would turn a child exit of 1 (256) into
  // success when a translator returns it from main and the OS keeps 8 bits.
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  std::fputs("waitpid returned a nonterminal child status\n", stderr);
  std::abort();
}

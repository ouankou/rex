#include "processSupport.h"

#include <csignal>
#include <cstdlib>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

namespace {
void require(bool condition) {
  if (!condition)
    std::abort();
}
volatile sig_atomic_t interrupts = 0;
void onAlarm(int) { interrupts = 1; }
} // namespace

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "exit")
    return std::stoi(argv[2]);
  if (argc == 2 && std::string(argv[1]) == "signal") {
    raise(SIGTERM);
    return 1;
  }
  if (argc == 2 && std::string(argv[1]) == "delay") {
    timespec delay{0, 100000000};
    require(nanosleep(&delay, nullptr) == 0);
    return 0;
  }
  const std::string self = argv[0];
  for (int code : {0, 1, 42, 255})
    require(systemFromVector({self, "exit", std::to_string(code)}) == code);
  require(systemFromVector({self, "signal"}) == 128 + SIGTERM);
  require(systemFromVector({"/rex/nonexistent/executable"}) == 127);

  struct sigaction action{};
  action.sa_handler = onAlarm;
  sigemptyset(&action.sa_mask);
  struct sigaction previous{};
  require(sigaction(SIGALRM, &action, &previous) == 0);
  itimerval timer{};
  timer.it_interval.tv_usec = 10000;
  timer.it_value = timer.it_interval;
  require(setitimer(ITIMER_REAL, &timer, nullptr) == 0);
  const int result = systemFromVector({self, "delay"});
  timer = {};
  require(setitimer(ITIMER_REAL, &timer, nullptr) == 0);
  require(sigaction(SIGALRM, &previous, nullptr) == 0);
  require(interrupts != 0 && result == 0);
}

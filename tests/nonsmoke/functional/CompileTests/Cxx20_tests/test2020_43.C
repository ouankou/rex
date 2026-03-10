#include "test2020_coroutine_support.hpp"

generator<int> iota(int n = 0) {
  while (true) {
    co_yield n++;
  }
}

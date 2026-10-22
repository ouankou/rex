#include <coroutine>

struct yield_awaitable {};

std::suspend_always operator co_await(yield_awaitable) noexcept { return {}; }

struct yielding_task {
  struct promise_type {
    yielding_task get_return_object() noexcept { return {}; }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
    yield_awaitable yield_value(int, int = 1) noexcept { return {}; }
  };
};

yielding_task yield_once() { co_yield 7; }

#ifndef REX_TEST2020_COROUTINE_SUPPORT_HPP
#define REX_TEST2020_COROUTINE_SUPPORT_HPP

#include <coroutine>
#include <cstddef>
#include <utility>

template <typename T = void> struct task {
  struct promise_type {
    task get_return_object() noexcept { return {}; }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    std::suspend_never final_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() const {}
  };
};

template <typename T> struct generator {
  struct promise_type {
    T current_value{};
    generator get_return_object() noexcept { return {}; }
    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }
    std::suspend_always yield_value(T value) noexcept {
      current_value = std::move(value);
      return {};
    }
    void return_void() const noexcept {}
    void unhandled_exception() const {}
  };
};

template <typename T> struct lazy {
  struct promise_type {
    T value{};
    lazy get_return_object() noexcept { return {}; }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    std::suspend_never final_suspend() const noexcept { return {}; }
    void return_value(T new_value) noexcept { value = std::move(new_value); }
    void unhandled_exception() const {}
  };
};

struct buffer_view {
  char *data;
  std::size_t size;
};

inline buffer_view buffer(char *data) noexcept { return {data, 1024}; }

inline buffer_view buffer(char *data, std::size_t size) noexcept {
  return {data, size};
}

template <typename T> struct ready_awaitable {
  T value{};
  bool await_ready() const noexcept { return true; }
  void await_suspend(std::coroutine_handle<>) const noexcept {}
  T await_resume() const noexcept { return value; }
};

template <> struct ready_awaitable<void> {
  bool await_ready() const noexcept { return true; }
  void await_suspend(std::coroutine_handle<>) const noexcept {}
  void await_resume() const noexcept {}
};

struct mock_socket {
  ready_awaitable<std::size_t> async_read_some(buffer_view) const noexcept {
    return {0};
  }
};

inline ready_awaitable<void> async_write(mock_socket &, buffer_view) noexcept {
  return {};
}

#endif

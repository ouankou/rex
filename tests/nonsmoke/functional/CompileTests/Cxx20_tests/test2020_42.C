#include "test2020_coroutine_support.hpp"

task<> tcp_echo_server(mock_socket &socket) {
  char data[1024];
  for (;;) {
    std::size_t n = co_await socket.async_read_some(buffer(data));
    co_await async_write(socket, buffer(data, n));
    break;
  }
}

#pragma once

#include <stdexcept>
#include <system_error>
#include <fmt/core.h>

#include <asio.hpp>

struct serial_iostream: asio::serial_port {
  using asio::serial_port::serial_port;

  void write(const char* s, int len) {
    asio::write(*this, asio::buffer(s, len));
  }

  void read(char* s, int len) {
    asio::read(*this, asio::buffer(s, len));
  }

  void flush() {
    int rc = tcflush(lowest_layer().native_handle(), TCOFLUSH);
    if(rc < 0) throw std::system_error(errno,
                                       std::system_category(),
                                       fmt::format("Flush failed , errno {}", errno));
  }
};

#include <asio.hpp>
#include <fmt/core.h>

int main() {
  asio::io_service service;
  asio::basic_serial_port stream(service, "/dev/serial0");
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  char buf[1024];

  int j = 0;
  while(stream.is_open()) {
    int read = stream.read_some(asio::buffer(buf));
    for(int i = 0; i < read; i++, j++) {
      if(j % 16 == 0) fmt::print("\n");
      fmt::print("{:2X}", buf[i]);
    }
  }

  return 0;
}

#include <asio.hpp>
#include <fstream>
#include <fmt/core.h>

#include <common/msg.h>
#include <common/serial.h>

void write_keyboard(const char* keyboard_file, keyboard_t& buf) {
  static std::ofstream out(keyboard_file, std::ios::out
                           | std::ios::binary
                           | std::ios::app);
  out.write((char*)buf.data(), buf.size());
  out.flush();

  fmt::print("kbd: ");
  for(auto& b: buf)
    fmt::print("{:2X}", b);
  fmt::print("\n");
}

void write_mouse(const char* mouse_file, mouse_t& buf) {
  static std::ofstream out(mouse_file, std::ios::out
                           | std::ios::binary
                           | std::ios::app);
  out.write((char*)buf.data(), buf.size());
  out.flush();

  fmt::print("mouse: ");
  for(auto& b: buf)
    fmt::print("{:2X}", b);
  fmt::print("\n");
}

void run_server(const char* serial_file,
                const char* keyboard_file,
                const char* mouse_file) {
  asio::io_service service;
  serial_iostream stream(service, serial_file);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  while(stream.is_open()) {
    int cmd = read_trivial<int>(stream);

    switch(cmd) {
      case CMD_KEYBOARD: {
        auto buf = read_trivial<keyboard_t>(stream);
        write_keyboard(keyboard_file, buf);
        break;
      }

      case CMD_MOUSE: {
        auto buf = read_trivial<mouse_t>(stream);
        write_mouse(mouse_file, buf);
        break;
      }
    }
  }
}

int main(int argc, char** argv) {
  if(argc < 4) {
    fmt::print("Usage: {} <serial file> <keyboard file> <mouse file>\n", argv[0]);
    return 1;
  }

  fmt::print("Starting server on serial device {}\n", argv[1]);
  fmt::print("Using keyboard file {}\n", argv[2]);
  fmt::print("Using mouse file {}\n", argv[3]);

  run_server(argv[1], argv[2], argv[3]);

  return 0;
}

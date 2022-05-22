// Some constants for communication
#pragma once

#include <array>
#include <cstdint>

enum : int{
  CMD_KEYBOARD,
  CMD_MOUSE,
};

typedef std::array<uint8_t, 8> keyboard_t;
typedef std::array<uint8_t, 4> mouse_t;

template <typename T, typename stream>
T read_trivial(stream& s) {
  T ret;
  s.read(reinterpret_cast<char*>(&ret), sizeof(T));
  return ret;
}

template <typename T, typename stream>
void write_trivial(stream& s, const T& t) {
  s.write(reinterpret_cast<const char*>(&t), sizeof(T));
}

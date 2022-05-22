#include "window.h"
#include "async_capture.h"
#include "keys.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <common/serial.h>

#include <asio.hpp>

#include <iostream>
#include <fmt/core.h>
#include <bitset>

int main(int argc, char** argv) {
  if(argc < 3) {
    fmt::print("USAGE: {} <v4l2 device> <serial device>\n", argv[0]);
    return 1;
  }

  keys::KeyState keys;
  asio::io_service service;
  serial_iostream stream(service, argv[2]);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  AsyncCapture cap(argv[1]);

  int w = cap->get_width(), h = cap->get_height();
  fmt::print("{}x{}\n", w, h);

  Window win(w, h);
  win.set_title("Harness");

  auto texture = win.create_texture(SDL_PIXELFORMAT_NV12, w, h);
  texture.set_scale_mode(SDL_ScaleModeBest);

  cap.start();

  bool running = true;
  while(running) {
    if(auto frame = cap.pop_frame()) {
      {
        auto pixels = texture.guard();
        std::copy(frame->data.begin(), frame->data.end(), pixels.data.begin());
        frame.reset();
      }

      auto [win_w, win_h] = win.get_dims();
      auto scale = std::min(double(win_w) / w, double(win_h) / h);

      win.render_clear();
      win.render_copy(texture, SDL_Rect{0, 0, int(scale * w), int(scale * h)});
      win.render_present();
    }

    win.process_events([&](const SDL_Event& e) {
      if(e.type == SDL_QUIT) {
        running = false;
      }

      if(keys::OnPress(e, keys::kbd_button{SDL_SCANCODE_LCTRL}, keys::kbd_button{SDL_SCANCODE_RCTRL})
         && win.is_grabbed()) {
        fmt::print("mouse unlock\n");
        keys.reset(stream);
        win.set_grab(false);
      }

      if(win.is_grabbed())
        keys.consume_event(e);

      if(keys::OnPress(e, keys::mouse_button{SDL_BUTTON_LEFT})
         && !win.is_grabbed()) {
        fmt::print("mouse lock\n");
        keys.reset(stream);
        win.set_grab(true);
      }
    });

    keys.dump(stream);
  }

  return 0;
}

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

ErrorOr<void> go(int argc, char** argv) {
  if(argc < 3) return Error::format("USAGE: {} <v4l2 device> <serial device>", argv[0]);

  keys::KeyState keys;
  asio::io_service service;
  serial_iostream stream(service, argv[2]);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  auto cap = TRY(AsyncCapture::open(argv[1]));

  int w = cap->get_width(), h = cap->get_height();
  fmt::print("{}x{}\n", w, h);

  auto win = TRY(Window::create(w, h));
  win.set_title("Harness");

  auto texture = TRY(win.create_texture(SDL_PIXELFORMAT_NV12, w, h));
  texture.set_scale_mode(SDL_ScaleModeBest);

  cap.start();

  bool running = true;
  while(running) {
    auto start = SDL_GetPerformanceCounter();

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

    auto end = SDL_GetPerformanceCounter();
    int elapsed_ms = (end - start) * 1000. / SDL_GetPerformanceFrequency();
    SDL_Delay(std::max(0, int((1000. / 120) - elapsed_ms)));

    keys.dump(stream);
  }

  return std::nullopt;
}

int main(int argc, char** argv) {
  auto ret = go(argc, argv);

  if(ret.is_error()) {
    fmt::print("{}\n", ret.error().what());
    return 1;
  }

  return 0;
}

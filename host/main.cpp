#include "window.h"
#include "capture.h"
#include "keys.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <common/serial.h>

#include <asio.hpp>

#include <iostream>
#include <fmt/core.h>
#include <bitset>
#include <thread>

int main(int argc, char** argv) {
  if(argc < 3) {
    fmt::print("USAGE: {} <v4l2 device> <serial device>\n", argv[0]);
    return 1;
  }

  keys::KeyState keys;
  asio::io_service service;
  serial_iostream stream(service, argv[2]);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  std::optional<Capture> cap;
  cap.emplace(argv[1]);
  cap->start(4);

  int w = cap->get_width(), h = cap->get_height();
  fmt::print("{}x{}\n", w, h);

  Window win(w, h);
  win.set_title("Harness");

  auto texture = win.create_texture(SDL_PIXELFORMAT_NV12, w, h);
  texture.set_scale_mode(SDL_ScaleModeBest);

  std::atomic<bool> have_frame = 0;
  std::optional<Capture::BufferHandle> frame;

  bool running = true;
  std::jthread cap_thread([&]{
    while(running) {
      try {
        if(!cap) {
          cap.emplace(argv[1]);
          cap->start(4);
        }

        while(running) {
          if(!have_frame.load(std::memory_order_acquire)) {
            auto maybe_frame = cap->read_frame();
            if(maybe_frame) {
              frame.emplace(std::move(*maybe_frame));
              have_frame.store(1, std::memory_order_release);
            }
          }
        }
      } catch(const std::system_error& e) {
        if(e.code() != std::error_code(ENODEV, std::system_category()))
          throw;

        fmt::print("Capture card connection lost, attempting reconnect.\n");

        frame.reset();
        cap.reset();
        have_frame.store(0);
      }
    }
  });

  while(running) {
    if(have_frame.load(std::memory_order_acquire)) {
      {
        auto pixels = texture.guard();
        std::copy(frame->data.begin(), frame->data.end(), pixels.data.begin());
        frame.reset();
      }

      have_frame.store(false, std::memory_order_release);

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

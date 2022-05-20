#include "window.h"
#include "capture.h"
#include "keys.h"
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

  KeyState keys;
  asio::io_service service;
  serial_iostream stream(service, argv[2]);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  Capture cap(argv[1]);
  cap.start(4);

  int w = cap.get_width(), h = cap.get_height();
  fmt::print("{}x{}\n", w, h);

  Window win(w, h);

  auto texture = win.create_texture(SDL_PIXELFORMAT_NV12, w, h);
  texture.set_scale_mode(SDL_ScaleModeBest);

  std::atomic<bool> have_frame = 0;
  std::optional<Capture::BufferHandle> frame;

  bool running = true;
  std::jthread cap_thread([&]{
    while(running) {
      if(!have_frame.load(std::memory_order_acquire)) {
        auto maybe_frame = cap.read_frame();
        if(maybe_frame) {
          frame.emplace(std::move(*maybe_frame));
          have_frame.store(1, std::memory_order_release);
        }
      }
    }
  });

  std::bitset<2> ctrl_pressed {};
  bool mouse_pressed {};
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

    auto last_ctrl_pressed = ctrl_pressed;
    auto last_mouse_pressed = mouse_pressed;

    win.process_events([&](const SDL_Event& e) {
      if(e.type == SDL_QUIT) {
        running = false;
      } else if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        if(e.key.keysym.scancode == SDL_SCANCODE_LCTRL || e.key.keysym.scancode == SDL_SCANCODE_RCTRL)
          ctrl_pressed[e.key.keysym.scancode == SDL_SCANCODE_RCTRL] = e.type == SDL_KEYDOWN;
      } else if(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        if(e.button.button == SDL_BUTTON_LEFT)
          mouse_pressed = e.type == SDL_MOUSEBUTTONDOWN;
      }

      if(win.is_grabbed())
        keys.consume_event(e);
    });

    if(win.is_grabbed() && last_ctrl_pressed != 3 && ctrl_pressed == 3) {
      fmt::print("mouse unlock\n");
      keys.reset(stream);
      win.set_grab(false);
    }

    if(!win.is_grabbed() && mouse_pressed == 1 && last_mouse_pressed != 1) {
      fmt::print("mouse lock\n");
      keys.reset(stream);
      win.set_grab(true);
    }

    keys.dump(stream);
  }

  return 0;
}

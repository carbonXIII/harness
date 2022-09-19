#include "window.h"
#include "async_capture.h"
#include "keys.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <common/serial.h>

#include <asio.hpp>

#include <iostream>
#include <thread>
#include <fmt/core.h>
#include <bitset>

ErrorOr<AsyncCapture> open_capture_with_timeout(const char* path, std::chrono::system_clock::duration timeout) {
  std::optional<AsyncCapture> cap;
  std::optional<Error> last_err;

  auto start = std::chrono::system_clock::now();
  while(!cap && std::chrono::system_clock::now() - start < timeout) {
    auto res = AsyncCapture::open(path);

    if(res.is_error()) {
      fmt::print("Failed to open capture device: {}\n", res.error().what());
      std::this_thread::sleep_for(std::chrono::seconds(2));

      last_err = res.error();
    } else {
      cap.emplace(res.release_value());
    }
  }

  if(!cap) return Error::format("Timed out while opening capture: {}\n", last_err->what());
  return std::move(cap.value());
}

ErrorOr<void> go(int argc, char** argv) {
  if(argc < 3) return Error::format("USAGE: {} <v4l2 device> <serial device>", argv[0]);

  keys::KeyState keys;
  asio::io_service service;
  serial_iostream stream(service, argv[2]);
  stream.set_option(asio::serial_port_base::baud_rate(115200));

  auto cap = TRY(open_capture_with_timeout(argv[1], std::chrono::seconds(30)));

  int w = cap->get_width(), h = cap->get_height();
  fmt::print("{}x{}\n", w, h);

  auto win = TRY(Window::create(w, h));
  win.set_title("Harness");

  auto texture = TRY(win.create_texture(SDL_PIXELFORMAT_NV12, w, h));
  texture.set_scale_mode(SDL_ScaleModeBest);

  cap.start();

  bool running = true;

  int scaled_w, scaled_h;
  std::tie(scaled_w, scaled_h) = win.get_dims();

  while(running) {
    auto start = SDL_GetPerformanceCounter();

    auto [win_w, win_h] = win.get_dims();

    if(auto frame = cap.pop_frame()) {
      {
        auto pixels = texture.guard();
        std::copy(frame->data.begin(), frame->data.end(), pixels.data.begin());
        frame.reset();
      }

      auto scale = std::min(double(win_w) / w, double(win_h) / h);

      scaled_w = w * scale;
      scaled_h = h * scale;

      win.render_clear();
      win.render_copy(texture, SDL_Rect{0, 0, scaled_w, scaled_h});
      win.render_present();
    }

    win.process_events([&](const SDL_Event& e) {
      if(e.type == SDL_QUIT) running = false;
      keys.consume_event(e, scaled_w, scaled_h);
    });

    keys.dump(stream);

    auto end = SDL_GetPerformanceCounter();
    int elapsed_ms = (end - start) * 1000. / SDL_GetPerformanceFrequency();
    SDL_Delay(std::max(0, int((1000. / 120) - elapsed_ms)));
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

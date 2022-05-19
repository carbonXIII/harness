#include "window.h"
#include "capture.h"
#include "keys.h"

#include <fstream>

#include <fmt/core.h>

int main(int argc, char** argv) {
  if(argc < 3) {
    fmt::print("USAGE: {} <v4l2 device> <serial device>\n", argv[0]);
    return 1;
  }

  KeyState keys;
  std::ofstream stream(argv[2]);

  Capture cap(argv[1]);
  cap.start(4);

  int w = cap.get_width(), h = cap.get_height();
  fmt::print("{}x{}\n", w, h);

  Window win(w, h);

  auto texture = win.create_texture(SDL_PIXELFORMAT_NV12, w, h);

  auto process_frame = [&texture](const std::span<std::byte>& frame) {
    if(frame.empty()) return false;

    auto pixels = texture.guard();
    std::copy(frame.begin(), frame.end(), pixels.data.begin());
    return true;
  };

  bool running = true;
  while(running) {
    if(cap.read_frame<bool>(process_frame)) {
      win.render_copy(texture);
      win.render_present();
    }

    win.process_events([&](const SDL_Event& e) {
      if(e.type == SDL_QUIT)
        running = false;

      keys.consume_event(e, win.is_focused());
    });

    keys.dump(stream);
  }

  return 0;
}

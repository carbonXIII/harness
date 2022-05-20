#include <common/msg.h>

#include <SDL2/SDL.h>
#include <fmt/core.h>
#include <asio.hpp>

#include <bitset>
#include <set>
#include <chrono>

namespace keys {
  struct kbd_button {
    int scancode;
    bool operator!=(const SDL_Event& e) {
      return (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) || e.key.keysym.scancode != scancode;
    }
  };

  struct mouse_button {
    int button;
    bool operator!=(const SDL_Event& e) {
      return (e.type != SDL_MOUSEBUTTONDOWN && e.type != SDL_MOUSEBUTTONUP) || e.button.button != button;
    }
  };

  template <typename = decltype([]{})>
  bool OnPress(const SDL_Event& e, auto... keys) {
    static std::bitset<sizeof...(keys)> mask {};

    int idx = 0;
    bool not_found = ((++idx, keys != e) && ...);

    bool last = mask.count() == sizeof...(keys);
    if(!not_found) mask[idx-1] = e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN;

    return !last && mask.count() == sizeof...(keys);
  }

  struct KeyState {
    using my_clock = std::chrono::high_resolution_clock;
    static constexpr my_clock::duration frame_time = std::chrono::milliseconds(8);

    enum : int {
      LCTRL = 0,
      LSHIFT,
      LALT,
      LGUI,
      RCTRL,
      RSHIFT,
      RALT,
      RGUI,
    };

    enum : int {
      MLEFT = 0,
      MRIGHT,
      MMIDDLE,
    };

    std::bitset<8> mod;
    std::set<SDL_Scancode> keys;

    struct MouseState {
      int dx, dy;
      int sx, sy;
      std::bitset<8> buttons;

      MouseState()
        : dx(0), dy(0), sx(0), sy(0), buttons(0) {}
    } mouse;

    char have_keyboard, have_mouse_button, have_mouse_motion;
    my_clock::time_point last_mouse;

    KeyState(): mod(0), have_keyboard(0), have_mouse_button(0), have_mouse_motion(0) {}

    void consume(SDL_Scancode code, bool press) {
      switch(code) {
        case SDL_SCANCODE_LCTRL: mod[LCTRL] = press; break;
        case SDL_SCANCODE_LSHIFT: mod[LSHIFT] = press; break;
        case SDL_SCANCODE_LALT: mod[LALT] = press; break;
        case SDL_SCANCODE_LGUI: mod[LGUI] = press; break;
        case SDL_SCANCODE_RCTRL: mod[RCTRL] = press; break;
        case SDL_SCANCODE_RSHIFT: mod[RSHIFT] = press; break;
        case SDL_SCANCODE_RALT: mod[LGUI] = press; break;
        case SDL_SCANCODE_RGUI: mod[RGUI] = press; break;

        default:
          if(press) keys.insert(code);
          else keys.erase(code);
      }

      have_keyboard = 1;
    }

    void consume_mouse_motion(int dx, int dy) {
      mouse.dx += dx;
      mouse.dy += dy;
      have_mouse_button = 1;
    }

    void consume_mouse_button(int code, bool press) {
      // TODO: forward and back buttons
      switch(code) {
        case SDL_BUTTON_LEFT: mouse.buttons[MLEFT] = press; break;
        case SDL_BUTTON_RIGHT: mouse.buttons[MRIGHT] = press; break;
        case SDL_BUTTON_MIDDLE: mouse.buttons[MMIDDLE] = press; break;
      }

      have_mouse_motion = 1;
    }

    void consume_mouse_wheel(int sx, int sy) {
      mouse.sx += sx;
      mouse.sy += sy;
      have_mouse_motion = 1;
    }

    KeyState& reset(auto& stream) {
      keys.clear();
      mod = {};
      mouse = {};

      dump(stream, true);

      return *this;
    }

    keyboard_t get_keyboard_buffer() {
      keyboard_t ret;
      ret[0] = mod.to_ulong();
      ret[1] = 0;

      if(keys.size() > 6) {
        // phantom condition, idk if this is needed
        for(int i = 2; i < 8; i++)
          ret[i] = 1;
      } else {
        int idx = 2;
        for(auto& sm: keys)
          ret[idx++] = sm;
        for(; idx < 6; idx++)
          ret[idx] = 0;
      }

      have_keyboard = 0;
      return ret;
    }

    mouse_t get_mouse_buffer() {
      mouse_t ret;

      ret[0] = mouse.buttons.to_ulong();
      ret[1] = (char)mouse.dx;
      ret[2] = (char)mouse.dy;
      ret[3] = (char)mouse.sy;

      // FIXME: no horizontal scroll for now
      // ret[4] = (char)mouse.sx;

      mouse.dx = mouse.dy = mouse.sx = mouse.sy = 0;

      have_mouse_button = have_mouse_motion = 0;
      last_mouse = my_clock::now();

      return ret;
    }

    bool keyboard_ready() {
      return have_keyboard;
    }

    bool mouse_ready() {
      return have_mouse_button || (have_mouse_motion && my_clock::now() - last_mouse > frame_time);
    }

    void consume_event(const SDL_Event& event) {
      if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        consume(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
      }

      // else if(event.type = sdl_mousemotion
      //           || event.type == sdl_mousebuttondown
      //           || event.type == sdl_mousebuttonup
      //           || event.type == sdl_mousewheel) {
      //   std::cerr << "here\n";
      // }

      if(event.type == SDL_MOUSEMOTION) {
        consume_mouse_motion(event.motion.xrel, event.motion.yrel);
      }

      else if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        consume_mouse_button(event.button.button, event.type == SDL_MOUSEBUTTONDOWN);
      }

      else if(event.type == SDL_MOUSEWHEEL) {
        consume_mouse_wheel(event.wheel.x, event.wheel.y);
      }
    }

    void dump(auto& stream, bool force = false) {
      int wrote = 0;

      if(force || keyboard_ready()) {
        auto buf = get_keyboard_buffer();

        {
          fmt::print("kbd: ");
          for(auto b: buf) fmt::print("{:02X}", b);
          fmt::print("\n");
        }

        write_trivial<int>(stream, CMD_KEYBOARD);
        write_trivial(stream, buf);
        wrote++;
      }

      if(force || mouse_ready()) {
        auto buf = get_mouse_buffer();

        {
          fmt::print("mouse: ");
          for(auto b: buf) fmt::print("{:02X}", b);
          fmt::print("\n");
        }

        write_trivial<int>(stream, CMD_MOUSE);
        write_trivial(stream, buf);
        wrote++;
      }

      if(wrote)
        stream.flush();
    }
  };
}

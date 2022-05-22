#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <span>
#include <utility>
#include <stdexcept>
#include <optional>

#include <SDL2/SDL.h>

struct Window {
  struct Texture {
    Texture(const Texture& o) = delete;
    Texture(Texture&& o): texture(std::exchange(o.texture, nullptr)) {}
    ~Texture() { if(texture) SDL_DestroyTexture(texture); }

    void set_scale_mode(SDL_ScaleMode mode) {
      SDL_SetTextureScaleMode(texture, mode);
    }

  protected:
    friend struct Window;
    friend struct Guard;

    struct Guard {
      Guard(Texture* parent) : parent(parent), data(parent->lock()) {}
      Guard(const Guard& o) = delete;
      ~Guard() { parent->unlock(); }

      const std::span<std::byte> data;
      int pitch;
    protected:
      Texture* parent;
    };

    Texture(SDL_Texture* texture, int width, int height)
      : texture(texture), width(width), height(height) {}

    std::span<std::byte> lock() {
      void* mem;
      int pitch;

      // TODO: lock subrect?
      SDL_LockTexture(texture, nullptr, &mem, &pitch);
      return {static_cast<std::byte*>(mem), size_t(pitch * height)};
    }

    void unlock() { SDL_UnlockTexture(texture); }

    SDL_Texture* texture;
    int width;
    int height;

  public:
    Guard guard() { return Guard(this); }
  };

  Window(int width, int height) {
    static auto _ = []() {
      int rc = SDL_Init(SDL_INIT_VIDEO);
      if(rc != 0)
        throw std::runtime_error(SDL_GetError());
      return rc;
    }();

    int rc = SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_RESIZABLE, &win, &render);
    if(rc < 0) throw std::runtime_error(SDL_GetError());
  }

  Window(const Window&) = delete;

  Texture create_texture(uint32_t format, int width, int height) {
    auto* ret = SDL_CreateTexture(render, format, SDL_TEXTUREACCESS_STREAMING, width, height);
    if(ret == nullptr) throw std::runtime_error(SDL_GetError());
    return Texture(ret, width, height);
  }

  void render_clear() {
    SDL_RenderClear(render);
  }

  void render_copy(const Texture& texture, std::optional<SDL_Rect> dst = std::nullopt) {
    // TODO: src, dst rects
    SDL_RenderCopy(render, texture.texture, nullptr, dst ? &dst.value() : nullptr);
  }

  void render_present() { SDL_RenderPresent(render); }

  void process_events(auto&& f) {
    SDL_Event e;
    while(SDL_PollEvent(&e))
      f(e);
  }

  bool is_grabbed() {
    auto state = SDL_GetWindowFlags(win);
    return state & SDL_WINDOW_INPUT_GRABBED;
  }

  void set_grab(bool grab) {
    SDL_SetWindowGrab(win, grab ? SDL_TRUE : SDL_FALSE);
    SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);
  }

  void set_title(const std::string& s) {
    SDL_SetWindowTitle(win, s.c_str());
  }

  std::pair<int,int> get_dims() {
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    return {w, h};
  }

  ~Window() {
    if(win && render) {
      SDL_DestroyWindow(win);
      SDL_DestroyRenderer(render);
    }
  }

protected:
  SDL_Window* win;
  SDL_Renderer* render;
};

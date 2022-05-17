#include <span>
#include <utility>
#include <stdexcept>

#include <SDL2/SDL.h>

struct Window {
  struct Texture {
    Texture(const Texture& o) = delete;
    Texture(Texture&& o): texture(std::exchange(o.texture, nullptr)) {}
    ~Texture() { if(texture) SDL_DestroyTexture(texture); }

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

  void render_copy(const Texture& texture) {
    // TODO: src, dst rects
    SDL_RenderCopy(render, texture.texture, nullptr, nullptr);
  }

  void render_present() { SDL_RenderPresent(render); }

  void process_events(auto&& f) {
    SDL_Event e;
    while(SDL_PollEvent(&e))
      f(e);
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

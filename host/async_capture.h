#pragma once

#include "capture.h"

#include <fmt/core.h>
#include <optional>
#include <atomic>
#include <thread>

struct AsyncCapture {
  using BufferHandle = Capture::BufferHandle;

  AsyncCapture(const char* device): device(device) {
    init();
  }

  std::optional<BufferHandle> pop_frame() {
    if(have_frame.load(std::memory_order_acquire)) {
      std::optional<BufferHandle> ret(std::move(frame));
      frame.reset();

      have_frame.store(0, std::memory_order_release);

      return ret;
    }

    return std::nullopt;
  }

  Capture* operator->() {
    return &cap.value();
  }

  void start() {
    running = true;
    thread = std::jthread([this](){ this->run(); });
  }

  void stop() {
    running = false;
  }

  ~AsyncCapture() {
    stop();
  }

protected:
  const char* device;

  std::optional<Capture> cap;

  std::atomic<bool> have_frame = false;
  std::optional<BufferHandle> frame;

  bool running = false;
  std::jthread thread;

  void init() {
    cap.emplace(device);
    cap->start(4);
  }

  void run() {
    while(running) {
      try {
        if(!cap) init();

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
  }
};

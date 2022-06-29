#pragma once

#include "capture.h"

#include <common/err.h>
#include <fmt/core.h>
#include <optional>
#include <atomic>
#include <thread>

struct AsyncCapture {
  using BufferHandle = Capture::BufferHandle;

  AsyncCapture(AsyncCapture&& o):
    device((o.join(), o.device)), // make sure we join before we do anything else
    cap(std::exchange(o.cap, std::nullopt)) {}

  static ErrorOr<AsyncCapture> open(const char* device) {
    AsyncCapture ret(device);
    TRY(ret.init());
    return std::move(ret);
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

  void join() {
    stop();
    if(thread.joinable())
      thread.join();
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

  AsyncCapture(const char* device): device(device) {}

  ErrorOr<void> init() {
    cap.emplace(TRY(Capture::open(device)));
    TRY(cap->start(4));
    return {};
  }

  void run() {
    while(running) {
      auto err = [this]() -> ErrorOr<void> {
        if(!cap) TRY(init());

        while(running) {
          if(!have_frame.load(std::memory_order_acquire)) {
            auto maybe_frame = TRY(cap->read_frame());
            if(maybe_frame.has_value()) {
              frame.emplace(std::move(*maybe_frame));
              have_frame.store(1, std::memory_order_release);
            }
          }
        }

        return {};
      }();

      if(err.is_error()) {
        if(err.error().code == ENODEV) {
          fmt::print("Capture card connection lost, attempting reconnect: {}\n", err.error().what());

          frame.reset();
          cap.reset();
          have_frame.store(0);
        } else {
          fmt::print("In capture thread: {}\n", err.error().what());
          std::terminate();
        }
      }
    }
  }
};

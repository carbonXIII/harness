#pragma once

#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>
#include <span>
#include <fmt/core.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

static int do_ioctl(int fd, unsigned request, auto&& args) {
  int rc;
  do {
    rc = ioctl(fd, request, static_cast<void*>(&args));
  } while(rc < 0 && errno == EINTR);

  if(rc < 0 && errno != EAGAIN)
    throw std::system_error(errno,
                            std::system_category(),
                            fmt::format("Error during ioctl, request {}, errno {}", request, errno));

  return rc;
}

struct MMapSpan: std::span<std::byte> {
  using Base = std::span<std::byte>;
  using Base::Base;

  ~MMapSpan() {
    if(!empty()) munmap(data(), size());
  }
};

struct Capture {
  Capture(const char* path) {
    this->path = path;
    fd = ::open(path, O_RDWR, 0);
    if(fd < 0)
      throw std::system_error(errno,
                              std::system_category(),
                              fmt::format("Failed to open capture device, errno = {}", errno));

    v4l2_capability caps;
    do_ioctl(fd, VIDIOC_QUERYCAP, caps);

    int mask = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    if((caps.capabilities & mask) != mask)
      throw std::runtime_error("Capture does not support video capture and streaming.");

    do_ioctl(fd, VIDIOC_G_FMT, fmt);
    set_resolution(get_width(), get_height());
  }

  Capture& set_resolution(int width, int height) {
    fmt = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .fmt = {
        .pix = {
          .width = 2560,
          .height = 1440,
          .pixelformat = V4L2_PIX_FMT_NV12,
          .field = V4L2_FIELD_ANY
        }
      }
    };

    do_ioctl(fd, VIDIOC_S_FMT, fmt);
    return *this;
  }

  Capture& start(uint32_t buffer_count) {
    v4l2_requestbuffers req_buf = {
      .count = buffer_count,
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP
    };

    do_ioctl(fd, VIDIOC_REQBUFS, req_buf);

    if(req_buf.count < 2)
      throw std::runtime_error("Not enough buffers provided");

    buffers.reserve(req_buf.count);
    for(uint32_t i = 0; i < req_buf.count; i++) {
      v4l2_buffer buf = {
        .index = i,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
      };

      do_ioctl(fd, VIDIOC_QUERYBUF, buf);

      void* mem = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
      if(mem == MAP_FAILED)
        throw std::system_error(errno,
                                std::system_category(),
                                fmt::format("Failed to mmap buffer, errno = {}", errno));

      buffers.emplace_back(static_cast<std::byte*>(mem), buf.length);
      queue_buffer(i);
    }

    do_ioctl(fd, VIDIOC_STREAMON, V4L2_BUF_TYPE_VIDEO_CAPTURE);

    return *this;
  }

  void queue_buffer(uint32_t i) {
    v4l2_buffer buf = {
      .index = i,
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP
    };

    do_ioctl(fd, VIDIOC_QBUF, buf);
  }

  struct BufferHandle {
    Capture* parent;
    int index;
    std::span<std::byte> data;

    BufferHandle(Capture* parent, int index, std::span<std::byte> data)
      : parent(parent), index(index), data(data) {}
    BufferHandle(const BufferHandle& o) = delete;
    BufferHandle(BufferHandle&& o)
      : parent(o.parent), index(std::exchange(o.index, -1)), data(o.data) {}

    ~BufferHandle() {
      if(index >= 0) parent->queue_buffer(index);
    }
  };

  std::optional<BufferHandle> read_frame() {
    v4l2_buffer buf = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP,
    };

    if(do_ioctl(fd, VIDIOC_DQBUF, buf))
      return std::nullopt;

    if(buf.index < 0 || buf.index >= buffers.size())
      throw std::runtime_error(fmt::format("Dequeue'd buffer index out of range, {} not in [0, {})", buf.index, buffers.size()));

    return BufferHandle(this, buf.index, buffers[buf.index].subspan(0, buf.bytesused));
  }

  void stop() { do_ioctl(fd, VIDIOC_STREAMOFF, V4L2_BUF_TYPE_VIDEO_CAPTURE); }

  // TODO: cropping? VIDIOC_CROPCAP, VIDIOC_S_CROP

  Capture(const Capture& o) = delete;
  Capture(Capture&& o): path(o.path), fd(std::exchange(o.fd, 0)), fmt(o.fmt) {}
  ~Capture() { if(fd > 0) { stop(); close(fd); } }

  uint32_t get_width() const { return fmt.fmt.pix.width; }
  uint32_t get_height() const { return fmt.fmt.pix.height; }

protected:
  const char* path;
  int fd = 0;
  v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
  std::vector<MMapSpan> buffers;
};

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace mpsc {

static constexpr uint32_t PROTOCOL_VERSION = 1;
static constexpr size_t MSG_ALIGN = 16;

static constexpr uint32_t STATUS_EMPTY = 0;
static constexpr uint32_t STATUS_READY = 1;
static constexpr uint32_t STATUS_PADDING = 2;

struct MessageHeader {
  std::atomic<uint32_t> status;
  uint32_t type;
  uint32_t length;
  uint32_t total_size;
};
static_assert(sizeof(MessageHeader) == MSG_ALIGN);

struct Message {
  uint32_t type;
  std::vector<char> data;
};

struct alignas(64) QueueMeta {
  uint32_t protocol_version;
  uint32_t capacity;
  std::atomic<uint32_t> initialized;
};

struct RingBufferCtrl {
  alignas(64) std::atomic<size_t> write_pos;
  alignas(64) std::atomic<size_t> read_pos;
};

inline size_t AlignUp(size_t n, size_t a) {
  return (n + a - 1) & ~(a - 1);
}

namespace detail {

inline size_t ComputeShmSize(size_t capacity) {
  return sizeof(QueueMeta) + sizeof(RingBufferCtrl) + capacity;
}

inline QueueMeta *GetMeta(void *base) {
  return reinterpret_cast<QueueMeta *>(base);
}

inline RingBufferCtrl *GetCtrl(void *base) {
  return reinterpret_cast<RingBufferCtrl *>(static_cast<char *>(base) +
                                            sizeof(QueueMeta));
}

inline char *GetBuffer(void *base) {
  return static_cast<char *>(base) + sizeof(QueueMeta) +
         sizeof(RingBufferCtrl);
}

} // namespace detail

class ProducerNode {
public:
  ProducerNode(const std::string &name, size_t capacity)
      : m_name(name), m_capacity(AlignUp(capacity, MSG_ALIGN)) {
    m_shm_size = detail::ComputeShmSize(m_capacity);

    shm_unlink(name.c_str());
    m_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    ftruncate(m_fd, static_cast<off_t>(m_shm_size));
    m_base = mmap(nullptr, m_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                  m_fd, 0);

    std::memset(m_base, 0, m_shm_size);

    auto *meta = detail::GetMeta(m_base);
    meta->protocol_version = PROTOCOL_VERSION;
    meta->capacity = static_cast<uint32_t>(m_capacity);

    m_ctrl = detail::GetCtrl(m_base);
    new (&m_ctrl->write_pos) std::atomic<size_t>(0);
    new (&m_ctrl->read_pos) std::atomic<size_t>(0);

    m_buf = detail::GetBuffer(m_base);

    meta->initialized.store(1, std::memory_order_release);
  }

  ~ProducerNode() {
    munmap(m_base, m_shm_size);
    close(m_fd);
    shm_unlink(m_name.c_str());
  }

  ProducerNode(const ProducerNode &) = delete;
  ProducerNode &operator=(const ProducerNode &) = delete;

  bool Send(uint32_t type, const void *data, uint32_t length) {
    size_t msg_size = AlignUp(sizeof(MessageHeader) + length, MSG_ALIGN);

    if (msg_size > m_capacity) {
      return false;
    }

    for (;;) {
      size_t wpos = m_ctrl->write_pos.load(std::memory_order_relaxed);
      size_t rpos = m_ctrl->read_pos.load(std::memory_order_acquire);

      size_t offset = wpos % m_capacity;
      size_t tail_space = m_capacity - offset;

      size_t total_advance;
      size_t data_offset;

      if (tail_space < msg_size) {
        total_advance = tail_space + msg_size;
        data_offset = 0;
      } else {
        total_advance = msg_size;
        data_offset = offset;
      }

      if (wpos + total_advance - rpos > m_capacity) {
        return false;
      }

      if (!m_ctrl->write_pos.compare_exchange_weak(wpos, wpos + total_advance,
                                                   std::memory_order_relaxed)) {
        continue;
      }

      if (tail_space < msg_size) {
        auto *pad = reinterpret_cast<MessageHeader *>(m_buf + offset);
        pad->type = 0;
        pad->length = 0;
        pad->total_size = static_cast<uint32_t>(tail_space);
        pad->status.store(STATUS_PADDING, std::memory_order_release);
      }

      auto *hdr = reinterpret_cast<MessageHeader *>(m_buf + data_offset);
      hdr->type = type;
      hdr->length = length;
      hdr->total_size = static_cast<uint32_t>(msg_size);
      if (length > 0) {
        std::memcpy(m_buf + data_offset + sizeof(MessageHeader), data, length);
      }
      hdr->status.store(STATUS_READY, std::memory_order_release);

      return true;
    }
  }

private:
  std::string m_name;
  size_t m_capacity;
  size_t m_shm_size{};
  int m_fd = -1;
  void *m_base = nullptr;
  RingBufferCtrl *m_ctrl = nullptr;
  char *m_buf = nullptr;
};

class ConsumerNode {
public:
  ConsumerNode(const std::string &name, size_t capacity)
      : m_capacity(AlignUp(capacity, MSG_ALIGN)) {
    m_shm_size = detail::ComputeShmSize(m_capacity);

    m_fd = shm_open(name.c_str(), O_RDWR, 0666);
    m_base = mmap(nullptr, m_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                  m_fd, 0);

    auto *meta = detail::GetMeta(m_base);
    while (meta->initialized.load(std::memory_order_acquire) != 1) {
      usleep(100);
    }

    m_ctrl = detail::GetCtrl(m_base);
    m_buf = detail::GetBuffer(m_base);
  }

  ~ConsumerNode() {
    munmap(m_base, m_shm_size);
    close(m_fd);
  }

  ConsumerNode(const ConsumerNode &) = delete;
  ConsumerNode &operator=(const ConsumerNode &) = delete;

  std::optional<Message> Receive() {
    for (;;) {
      size_t rpos = m_ctrl->read_pos.load(std::memory_order_relaxed);
      size_t wpos = m_ctrl->write_pos.load(std::memory_order_acquire);

      if (rpos == wpos) {
        return std::nullopt;
      }

      size_t offset = rpos % m_capacity;
      auto *hdr = reinterpret_cast<MessageHeader *>(m_buf + offset);

      uint32_t st = hdr->status.load(std::memory_order_acquire);
      if (st == STATUS_EMPTY) {
        return std::nullopt;
      }

      if (st == STATUS_PADDING) {
        size_t pad_size = hdr->total_size;
        hdr->status.store(STATUS_EMPTY, std::memory_order_release);
        m_ctrl->read_pos.store(rpos + pad_size, std::memory_order_release);
        continue;
      }

      Message msg;
      msg.type = hdr->type;
      msg.data.assign(m_buf + offset + sizeof(MessageHeader),
                      m_buf + offset + sizeof(MessageHeader) + hdr->length);

      size_t total = hdr->total_size;
      hdr->status.store(STATUS_EMPTY, std::memory_order_release);
      m_ctrl->read_pos.store(rpos + total, std::memory_order_release);

      return msg;
    }
  }

private:
  size_t m_capacity;
  size_t m_shm_size{};
  int m_fd = -1;
  void *m_base = nullptr;
  RingBufferCtrl *m_ctrl = nullptr;
  char *m_buf = nullptr;
};

}
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
static constexpr size_t MAX_MESSAGE_SIZE = 1024;
static constexpr size_t CACHE_LINE_SIZE = 64;

struct MessageHeader {
  uint32_t type;
  uint32_t length;
};

struct alignas(CACHE_LINE_SIZE) Cell {
  std::atomic<size_t> sequence;
  MessageHeader header;
  char data[MAX_MESSAGE_SIZE];
};

struct alignas(CACHE_LINE_SIZE) QueueMeta {
  uint32_t protocol_version;
  uint32_t capacity;
  uint32_t max_msg_size;
  std::atomic<uint32_t> initialized;
};

struct RingBufferCtrl {
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> enqueue_pos;
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> dequeue_pos;
};

struct Message {
  uint32_t type;
  std::vector<char> data;
};

namespace detail {

inline size_t ComputeShmSize(size_t capacity) {
  return sizeof(QueueMeta) + sizeof(RingBufferCtrl) + capacity * sizeof(Cell);
}

inline QueueMeta *GetMeta(void *base) {
  return reinterpret_cast<QueueMeta *>(base);
}

inline RingBufferCtrl *GetCtrl(void *base) {
  return reinterpret_cast<RingBufferCtrl *>(static_cast<char *>(base) +
                                            sizeof(QueueMeta));
}

inline Cell *GetCells(void *base) {
  return reinterpret_cast<Cell *>(static_cast<char *>(base) +
                                  sizeof(QueueMeta) +
                                  sizeof(RingBufferCtrl));
}

}

class ProducerNode {
public:
  ProducerNode(const std::string &name, size_t capacity)
      : m_name(name), m_capacity(capacity) {
    m_shm_size = detail::ComputeShmSize(capacity);

    shm_unlink(name.c_str());
    m_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
    ftruncate(m_fd, static_cast<off_t>(m_shm_size));
    m_base = mmap(nullptr, m_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);

    std::memset(m_base, 0, m_shm_size);

    auto *meta = detail::GetMeta(m_base);
    meta->protocol_version = PROTOCOL_VERSION;
    meta->capacity = static_cast<uint32_t>(capacity);
    meta->max_msg_size = MAX_MESSAGE_SIZE;

    m_ctrl = detail::GetCtrl(m_base);
    new (&m_ctrl->enqueue_pos) std::atomic<size_t>(0);
    new (&m_ctrl->dequeue_pos) std::atomic<size_t>(0);

    m_cells = detail::GetCells(m_base);
    for (size_t i = 0; i < capacity; ++i) {
      new (&m_cells[i].sequence) std::atomic<size_t>(i);
    }

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
    for (;;) {
      size_t pos = m_ctrl->enqueue_pos.load(std::memory_order_relaxed);
      Cell *cell = &m_cells[pos % m_capacity];
      size_t seq = cell->sequence.load(std::memory_order_acquire);
      auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

      if (diff == 0) {
        if (m_ctrl->enqueue_pos.compare_exchange_weak(
                pos, pos + 1, std::memory_order_relaxed)) {
          cell->header.type = type;
          cell->header.length = length;
          if (length > 0) {
            std::memcpy(cell->data, data, length);
          }
          cell->sequence.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;
      }
    }
  }

private:
  std::string m_name;
  size_t m_capacity;
  size_t m_shm_size{};
  int m_fd = -1;
  void *m_base = nullptr;
  RingBufferCtrl *m_ctrl = nullptr;
  Cell *m_cells = nullptr;
};

class ConsumerNode {
public:
  ConsumerNode(const std::string &name, size_t capacity)
      : m_capacity(capacity) {
    m_shm_size = detail::ComputeShmSize(capacity);

    m_fd = shm_open(name.c_str(), O_RDWR, 0666);
    m_base = mmap(nullptr, m_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);

    auto *meta = detail::GetMeta(m_base);
    while (meta->initialized.load(std::memory_order_acquire) != 1) {
      usleep(100);
    }

    m_ctrl = detail::GetCtrl(m_base);
    m_cells = detail::GetCells(m_base);
  }

  ~ConsumerNode() {
    munmap(m_base, m_shm_size);
    close(m_fd);
  }

  ConsumerNode(const ConsumerNode &) = delete;
  ConsumerNode &operator=(const ConsumerNode &) = delete;

  std::optional<Message> Receive() {
    size_t pos = m_ctrl->dequeue_pos.load(std::memory_order_relaxed);
    Cell *cell = &m_cells[pos % m_capacity];
    size_t seq = cell->sequence.load(std::memory_order_acquire);
    auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

    if (diff == 0) {
      Message msg;
      msg.type = cell->header.type;
      msg.data.assign(cell->data, cell->data + cell->header.length);

      cell->sequence.store(pos + m_capacity, std::memory_order_release);
      m_ctrl->dequeue_pos.store(pos + 1, std::memory_order_relaxed);

      return msg;
    }

    return std::nullopt;
  }

private:
  size_t m_capacity;
  size_t m_shm_size{};
  int m_fd = -1;
  void *m_base = nullptr;
  RingBufferCtrl *m_ctrl = nullptr;
  Cell *m_cells = nullptr;
};

}
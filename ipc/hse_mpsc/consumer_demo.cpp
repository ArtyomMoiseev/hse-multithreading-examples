#include "mpsc_queue.hpp"

#include <cstring>
#include <format>
#include <iostream>

static constexpr uint32_t MSG_TEXT = 0;
static constexpr uint32_t MSG_NUMBER = 1;
static constexpr uint32_t MSG_STOP = 2;

int main(int argc, char *argv[]) {
  static constexpr auto QueueName = "/hse_mpsc_demo";
  static constexpr size_t QueueCapacity = 64;

  bool has_filter = false;
  uint32_t filter_type = 0;
  if (argc > 1) {
    has_filter = true;
    filter_type = static_cast<uint32_t>(std::stoul(argv[1]));
    std::cout << std::format("consumer: will filter by message type {}",
                             filter_type)
              << std::endl;
  }

  std::cout << std::format("consumer: opening queue '{}'", QueueName)
            << std::endl;

  mpsc::ConsumerNode consumer(QueueName, QueueCapacity);

  std::cout << "consumer: connected to queue" << std::endl;

  for (;;) {
    auto msg = consumer.Receive();

    if (!msg.has_value()) {
      usleep(1000);
      continue;
    }

    if (msg->type == MSG_STOP) {
      std::cout << "consumer: got stop signal, stopping" << std::endl;
      break;
    }

    if (has_filter && msg->type != filter_type) {
      continue;
    }

    if (msg->type == MSG_TEXT) {
      std::string text(msg->data.begin(), msg->data.end());
      std::cout << std::format("consumer: [text] '{}'", text) << std::endl;
    } else if (msg->type == MSG_NUMBER) {
      int64_t num{};
      std::memcpy(&num, msg->data.data(), sizeof(num));
      std::cout << std::format("consumer: [number] {}", num) << std::endl;
    } else {
      std::cout << std::format("consumer: [unknown type {}] {} bytes", msg->type, msg->data.size()) << std::endl;
    }
  }

  std::cout << "Consumer: exiting" << std::endl;
  return 0;
}
#include "mpsc_queue.hpp"

#include <format>
#include <iostream>
#include <thread>

static constexpr uint32_t MSG_TEXT = 0;
static constexpr uint32_t MSG_NUMBER = 1;
static constexpr uint32_t MSG_STOP = 2;

int main() {
  static constexpr auto QueueName = "/hse_mpsc_demo";
  static constexpr size_t QueueCapacity = 64;

  std::cout << std::format("producer: creating queue '{}' with capacity {}",
                           QueueName, QueueCapacity)
            << std::endl;

  mpsc::ProducerNode producer(QueueName, QueueCapacity);

  std::cout << "producer: queue ready, waiting consumer..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  for (int i = 0; i < 5; ++i) {
    std::string text = std::format("hello #{} from producer", i);
    while (!producer.Send(MSG_TEXT, text.data(),
                          static_cast<uint32_t>(text.size()))) {
      usleep(100);
    }
    std::cout << std::format("producer: sent text '{}'", text) << std::endl;
  }

  for (int64_t n = 100; n < 105; ++n) {
    while (!producer.Send(MSG_NUMBER, &n, sizeof(n))) {
      usleep(100);
    }
    std::cout << std::format("producer: sent number {}", n) << std::endl;
  }

  while (!producer.Send(MSG_STOP, nullptr, 0)) {
    usleep(100);
  }
  std::cout << "producer: stop signal sent" << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "producer: finishing work" << std::endl;
  return 0;
}
#include <linux/futex.h>
#include <sys/syscall.h>

#include <atomic>
#include <format>
#include <iostream>
#include <thread>
#include <vector>

void FutexWait(void* value, int expectedValue) {
    syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expectedValue, nullptr, nullptr, 0);
}

void FutexWake(void* value, int count) {
    syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

class Latch {
public:
    explicit Latch(const int count) : m_count{count} {}

    void CountDown() {
        const int oldValue = m_count.fetch_sub(1);

        if (oldValue == 1) {
            FutexWake(&m_count, std::numeric_limits<int>::max());
        }
    }

    void Wait() {
        while (true) {
            const int current = m_count.load();

            if (current == 0) {
                break;
            }

            FutexWait(&m_count, current);
        }
    }

    void ArriveAndWait() {
        CountDown();
        Wait();
    }

private:
    std::atomic<int> m_count{};
};

int main() {
    static constexpr int WorkersCount = 3;
    Latch workersFinished{WorkersCount};

    std::vector<std::jthread> workers;
    for (int worker{}; worker < WorkersCount; ++worker) {
        workers.emplace_back([worker, &workersFinished](){
            // Simulate hard work
            std::this_thread::sleep_for(std::chrono::seconds{WorkersCount - worker});
            std::cout << std::format("Worker #{} finished", worker + 1) << std::endl;
            workersFinished.ArriveAndWait();
            std::cout << std::format("Worker #{} woke up", worker + 1) << std::endl;
        });
    }

    workersFinished.Wait();

    std::cout << "All workers finished" << std::endl;

    return 0;
}

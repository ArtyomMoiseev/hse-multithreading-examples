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

class HseMutex {
public:
    void Lock() {
        while (m_state.exchange(1) != 0) {
            FutexWait(&m_state, 1);
        }
    }

    void Unlock() {
        m_state.store(0);
        FutexWake(&m_state, 1);
    }

    void lock() { Lock(); }
    void unlock() { Unlock(); }

private:
    std::atomic<int> m_state{0};
};

int main() {
    static constexpr int WorkersCount = 4;
    static constexpr int IncrementsPerWorker = 1'000'000;

    HseMutex mutex;
    int sharedCounter = 0;

    std::vector<std::jthread> workers;
    for (int workerIndex = 0; workerIndex < WorkersCount; ++workerIndex) {
        workers.emplace_back([workerIndex, &mutex, &sharedCounter]() {
            for (int increment = 0; increment < IncrementsPerWorker; ++increment) {
                mutex.Lock();
                ++sharedCounter;
                mutex.Unlock();
            }
            std::cout << std::format("Worker #{} finished", workerIndex + 1) << std::endl;
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const int expectedValue = WorkersCount * IncrementsPerWorker;
    std::cout << std::format("Result : {} (expected: {})", sharedCounter, expectedValue) << std::endl;

    if (sharedCounter == expectedValue) {
        std::cout << "OK: working correct!" << std::endl;
    } else {
        std::cout << "FAIL: detected data race!" << std::endl;
        return 1;
    }

    return 0;
}

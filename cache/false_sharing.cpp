#include <latch>
#include <vector>
#include <thread>

struct SharedState {
    int a{};
    int b{};
    int c{};
};

int main() {
    static constexpr int WorkerThreads = 3;

    SharedState state;
    std::latch latch{WorkerThreads};

    auto threadFunc = [&state, &latch](int& value) {
        latch.arrive_and_wait();
        for (int i = 0; i < 500'000'000; ++i) {
            ++value;
        }
    };

    std::vector<std::jthread> threads;
    threads.emplace_back(threadFunc, std::ref(state.a));
    threads.emplace_back(threadFunc, std::ref(state.b));
    threads.emplace_back(threadFunc, std::ref(state.c));

    return 0;
}
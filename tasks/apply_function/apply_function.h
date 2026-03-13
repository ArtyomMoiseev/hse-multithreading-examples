#pragma once

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T>& data, std::function<void(T&)> func, int threadCount) {
    size_t size = data.size();
    size_t actualThreads = std::min(static_cast<size_t>(threadCount), size);

    if (actualThreads <= 1) {
        for (auto& elm : data) {
            func(elm);
        }
        return;
    }

    size_t chunkSize = size / actualThreads;
    size_t remainder = size % actualThreads;

    std::vector<std::thread> threads;
    threads.reserve(actualThreads);

    size_t start = 0;
    for (size_t i = 0; i < actualThreads; ++i) {
        size_t end = start + chunkSize + (i < remainder ? 1 : 0);
        threads.emplace_back([&data, &func, start, end] {
            for (size_t j = start; j < end; ++j) {
                func(data[j]);
            }
        });
        start = end;
    }

    for (auto& t : threads) {
        t.join();
    }
}

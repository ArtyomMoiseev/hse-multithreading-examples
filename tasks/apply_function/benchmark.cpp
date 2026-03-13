#include "apply_function.h"

#include <cmath>

#include <benchmark/benchmark.h>

static void BM_LightweightSmall(benchmark::State& state) {
    int threadCount = state.range(0);
    std::vector<int> data(100);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1);
        ApplyFunction<int>(data, [](int& x) { x += 1; }, threadCount);
    }
}
BENCHMARK(BM_LightweightSmall)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

static void BM_HeavyLarge(benchmark::State& state) {
    int threadCount = state.range(0);
    std::vector<double> data(100'000);
    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 1.0);
        ApplyFunction<double>(data, [](double& x) {
            for (int i = 0; i < 50; ++i) {
                x = std::sin(x) + std::cos(x);
            }
        }, threadCount);
    }
}
BENCHMARK(BM_HeavyLarge)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();

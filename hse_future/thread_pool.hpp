#pragma once

#include "future.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

using Task = std::function<void()>;

class ThreadPool {
 public:
  explicit ThreadPool(int num_threads) {
    for (int i = 0; i < num_threads; ++i) {
      workers_.emplace_back(&ThreadPool::Run, this);
    }
  }

  ~ThreadPool() {
    stopped_.store(true);
    cv_.notify_all();
    for (auto& w : workers_) {
      w.join();
    }
  }

  template <typename F>
  auto Submit(F&& func) -> Future<decltype(func())> {
    using R = decltype(func());
    auto state = std::make_shared<SharedState<R>>();

    {
      std::lock_guard<std::mutex> lock(mtx_);
      tasks_.push_back([f = std::forward<F>(func), state]() mutable {
        state->SetValue(f());
      });
    }
    cv_.notify_one();

    return Future<R>(state);
  }

 private:
  void Run() {
    while (true) {
      Task task;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] {
          return !tasks_.empty() || stopped_.load();
        });
        if (stopped_ && tasks_.empty()) return;

        task = std::move(tasks_.front());
        tasks_.pop_front();
      }
      task();
    }
  }

  std::mutex mtx_;
  std::condition_variable cv_;
  std::list<Task> tasks_;
  std::vector<std::thread> workers_;
  std::atomic<bool> stopped_{false};
};
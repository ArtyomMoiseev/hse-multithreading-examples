#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>

template <typename T>
class SharedState {
 public:
  void SetValue(T val) {
    std::lock_guard<std::mutex> lock(mtx_);
    value_ = std::move(val);
    cv_.notify_all();
  }

  T Get() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return value_.has_value(); });
    return std::move(*value_);
  }

 private:
  std::mutex mtx_;
  std::condition_variable cv_;
  std::optional<T> value_;
};

template <typename T>
class Future {
 public:
  explicit Future(std::shared_ptr<SharedState<T>> state)
      : state_(std::move(state)) {}

  T Get() {
    return state_->Get();
  }

 private:
  std::shared_ptr<SharedState<T>> state_;
};
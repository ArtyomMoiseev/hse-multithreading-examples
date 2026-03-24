#include "thread_pool.hpp"

#include <iostream>

int main() {
  ThreadPool pool(2);

  auto f1 = pool.Submit([] { return 1 + 2; });
  auto f2 = pool.Submit([] { return std::string("Hi hse!"); });

  std::cout << "Sum: " << f1.Get() << "\n";
  std::cout << "String: " << f2.Get() << "\n";
}
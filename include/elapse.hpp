#pragma once
#include <chrono>
#include <functional>

namespace goxx {
std::chrono::steady_clock::duration elapse(std::function<void()> f) {
    auto t = std::chrono::steady_clock::now();
    f();
    return std::chrono::steady_clock::now() - t;
}
} // namespace goxx
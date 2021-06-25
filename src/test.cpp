#include "goxx.hpp"
#include <chrono>
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>

using namespace goxx;

std::chrono::steady_clock::duration fcount(std::function<void()> f) {
    auto t = std::chrono::steady_clock::now();
    f();
    return std::chrono::steady_clock::now() - t;
}

int main() {
    auto num_threads = std::thread::hardware_concurrency();
    for (auto v : std::vector<bool>{false})
        for (auto count : std::vector<int>{1000000})
            for (auto nth : std::vector<size_t>{1, num_threads})
                for (auto csize : std::vector<size_t>{0, 1}) {
                    Chan<int> c{csize};
                    auto d = fcount([&]() {
                        std::vector<int> collected;
                        std::mutex collected_mtx;
                        WaitGroup wg{};
                        if (nth <= 1) {
                            wg.go([&c, &count]() {
                                for (auto i = 0; i < count; i++) {
                                    c.push(i);
                                }
                                c.close();
                            });
                            wg.go(
                                [&c, &v, &collected, &collected_mtx, &count]() {
                                    for (auto n : c) {
                                        if (v) {
                                            collected_mtx.lock();
                                            collected.emplace_back(n);
                                            collected_mtx.unlock();
                                        }
                                    }
                                });

                        } else {
                            wg.together(
                                [&c, &count](size_t tidx, size_t nthds) {
                                    for (auto i = tidx; i < count; i += nthds) {
                                        c.push((int)i);
                                    }
                                },
                                [&c]() { c.close(); }, nth);
                            wg.together(
                                [&c, &v, &collected, &collected_mtx](size_t,
                                                                     size_t) {
                                    for (auto n : c) {
                                        if (v) {
                                            collected_mtx.lock();
                                            collected.emplace_back(n);
                                            collected_mtx.unlock();
                                        }
                                    }
                                },
                                nullptr, nth);
                        }
                        wg.wait();
                        if (v) {
                            sort(collected.begin(), collected.end());
                            for (auto i = 0; i < count; i++) {
                                if (collected[i] != i) {
                                    std::cout << "collected[i] != i, "
                                              << collected[i] << " != " << i
                                              << "\n";
                                    throw std::runtime_error("assert failed");
                                }
                            }
                        }
                    });
                    if (!v) {
                        std::cout << "threads " << nth << ", count " << count
                                  << ", chan size  " << csize << ", elapse "
                                  << d / std::chrono::milliseconds(1)
                                  << " ms\n";
                    }
                }

    return 0;
}
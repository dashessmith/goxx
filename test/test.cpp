#include "goxx/goxx.hpp"
#include <algorithm>
#include <any>
#include <chrono>
#include <cstdio>
#include <deque>
#include <iostream>
#include <map>
#include <mutex>

using namespace std;

using namespace goxx;

void test_chan() {
    Chan<void *> ch;
    auto num_threads = thread::hardware_concurrency();
    for (auto verbose : {true, false})
        for (auto count : {1000000})
            for (auto nth : {num_threads})
                for (size_t csize : {0, 1, 2, 4, 8, 16, 1024}) {
                    Chan<int> c{csize};
                    auto d = elapse([&]() {
                        vector<int> collected;
                        mutex collected_mtx;
                        WaitGroup wg{};
                        if (nth <= 1) {
                            wg.go([&c, &count]() {
                                for (auto i = 0; i < count; i++) {
                                    c.push(i);
                                }
                                c.close();
                            });
                            wg.go([&c, &verbose, &collected, &collected_mtx,
                                   &count]() {
                                for (auto n : c) {
                                    if (verbose) {
                                        collected_mtx.lock();
                                        collected.emplace_back(n);
                                        collected_mtx.unlock();
                                    }
                                }
                                cout << " consumer quit\n";
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
                                [&c, &verbose, &collected,
                                 &collected_mtx](size_t, size_t) {
                                    for (auto n : c) {
                                        if (verbose) {
                                            collected_mtx.lock();
                                            collected.emplace_back(n);
                                            collected_mtx.unlock();
                                        }
                                    }
                                },
                                nullptr, nth);
                        }
                        wg.wait();
                        cout << "wait group quit\n";
                        if (verbose) {
                            sort(collected.begin(), collected.end());
                            for (auto i = 0; i < count; i++) {
                                if (collected[i] != i) {
                                    cout << "collected[i] != i, "
                                         << collected[i] << " != " << i << "\n";
                                    throw runtime_error("assert failed");
                                }
                            }
                            if (collected.size() != count) {
                                throw runtime_error("not all collected");
                            }
                        }
                    });
                    if (!verbose) {
                        cout << "threads " << nth << ", count " << count
                             << ", chan size  " << csize << ", elapse "
                             << d / chrono::milliseconds(1) << " ms\n";
                    }
                }
}

void test_wg() {
    WaitGroup wg{};
    int x = 0;
    mutex mtx;
    // condition_variable cv;
    wg.together([&](size_t idx, size_t) {
        for (;;) {
            unique_lock<mutex> ul(mtx);
            if (x != idx) {
                continue;
            }
            x++;
            cout << " index " << idx << " quit\n";
            break;
        }
    });
}
void test_mt_sort_origin() {
    size_t N = 100000000;
    vector<size_t> vec(N);
    for (size_t i = 0; i < N; i++) {
        vec[i] = (size_t)rand() / N;
    }
    // vec = {1118, 314, 1634, 10, 998, 2102, 2345, 2332, 3186, 3225};
    cout << "start\n";
    auto d = elapse([&]() { sort(begin(vec), end(vec)); });
    cout << " elapse " << (d / chrono::milliseconds(1)) << "ms\n";
    if (!is_sorted(vec.begin(), vec.end())) {
        cout << " test failed, not sorted\n";
        for (auto v : vec) {
            cout << v << " ";
        }
        cout << "\n";
    } else {
        // cout << " elapse " << (d / chrono::milliseconds(1)) <<
        // "ms\n";
    }
}

void test_mt_sort() {
    size_t N = 10000;
    vector<size_t> vec(N);
    for (size_t i = 0; i < N; i++) {
        vec[i] = (size_t)rand() / N;
    }
    // vec = {1118, 314, 1634, 10, 998, 2102, 2345, 2332, 3186, 3225};
    cout << "start\n";
    auto d = elapse([&]() { mt_sort(begin(vec), end(vec)); });
    cout << " elapse " << (d / chrono::milliseconds(1)) << "ms\n";
    if (!is_sorted(vec.begin(), vec.end())) {
        cout << " test failed, not sorted\n";
        for (auto v : vec) {
            cout << v << " ";
        }
        cout << "\n";
    } else {
        // cout << " elapse " << (d / chrono::milliseconds(1)) <<
        // "ms\n";
    }
}

void test_priority_queue() {
    priority_queue<int> q;
    for (int i = 0; i < 10; i++)
        q.push(10 - i);
    for (; !q.empty();) {
        auto n = q.top();
        q.pop();
        cout << "n = " << n << "\n";
    }
}

void test_get() {
    class Shouter {
      public:
        Shouter() { cout << "im here\n"; }
        ~Shouter() { cout << "i am gone!!\n"; }
        void foo() {}
    };
    auto x = goxx::get<vector<Shouter>>({.tag = "123"});
    x->emplace_back();
    x = goxx::get<vector<Shouter>>({.tag = "123", .permanent = true});
    x = goxx::get<vector<Shouter>>({.tag = "123", .permanent = true});
    x = goxx::get<vector<Shouter>>({.tag = "123", .permanent = true});
    x->at(0).foo();
    x = goxx::get<vector<Shouter>>({.tag = "123"});
    x = goxx::get<vector<Shouter>>({.permanent = true});
    x = goxx::get<vector<Shouter>>({.tag = "123"});
    x = goxx::get<vector<Shouter>>({.tag = "123456"});
}

void test_any() {
    auto ch = make_shared<Chan<any>>(1024);
    auto wg = make_shared<WaitGroup>();
    goxx_defer([&]() { wg->wait(); });
    wg->go([&]() {
        for (auto i = 0; i < 10; i++) {
            switch (i) {
            case 0:
                ch->push(0);
                break;
            case 1:
                ch->push(vector<int>{1, 2, 3});
                break;
            case 2:
                ch->push(map<string, string>{{"123", "456"}});
                break;
            case 3:
                ch->push("123fdsa");
                break;
            case 4:
                ch->push("jfidjfidss"s);
                break;
            default:
                ch->push(i);
                break;
            }
        }
        ch->close();
    });

    wg->go([&]() {
        this_thread::sleep_for(3s);
        for (auto x : *ch) {
            if (x.type() == typeid(int)) {
                cout << "x=" << any_cast<int>(x) << "\n";
            } else if (x.type() == typeid(string)) {
                cout << "x=" << any_cast<string>(x) << "\n";
            } else {
                cout << "x=" << x.type().name() << "\n";
            }
        }
    });
}

int main() {
    // test_chan();
    //  test_mt_sort_origin();
    //  test_mt_sort();
    //  test_priority_queue();
    //  test_get();
    test_any();
    return 0;
}
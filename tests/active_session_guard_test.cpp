#include "../app/src/features/stream/ActiveSessionGuard.hpp"

#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

using artemis::stream::ActiveSessionGuard;

namespace {

void loadStoreStress() {
    ActiveSessionGuard<int> guard;
    std::atomic<bool> stop{false};
    assert(guard.load() == nullptr);

    int first = 1;
    int second = 2;
    guard.store(&first);
    assert(guard.load() == &first);

    // One writer flips the pointer while several readers load and dereference
    // under a ScopedRead. Without synchronization a torn read would surface an
    // impossible value; here every read must observe exactly one of the two.
    std::atomic<int> rebinds{0};

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                const auto read = guard.scopedRead();
                const int* p = read.value();
                assert(p == &first || p == &second);
            }
        });
    }

    for (int i = 0; i < 20000; ++i) {
        guard.store(&first);
        guard.store(&second);
        rebinds.fetch_add(1, std::memory_order_relaxed);
    }
    stop.store(true, std::memory_order_release);
    for (auto& th : readers)
        th.join();
    assert(rebinds.load() == 20000);
    assert(guard.load() == &second);
}

void boolConversion() {
    ActiveSessionGuard<int> guard;
    assert(!guard.load());
    int value = 42;
    guard.store(&value);
    assert(guard.scopedRead());  // explicit operator bool
}

} // namespace

int main() {
    loadStoreStress();
    boolConversion();
    return 0;
}
// These tests report failures through assert(). Keep them live in every
// build type (Release defines NDEBUG), including asserts with side effects.
#undef NDEBUG

#include "TailscaleRandom.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

using artemis::tailscale::secureRandomBytes;

namespace {

using Key = std::array<std::uint8_t, 32>;

Key draw() {
    Key key{};
    secureRandomBytes(key);
    return key;
}

void zeroLengthRequestIsANoOp() {
    std::array<std::uint8_t, 4> guard = {0xde, 0xad, 0xbe, 0xef};
    secureRandomBytes(std::span<std::uint8_t>(guard.data(), 0));
    assert((guard == std::array<std::uint8_t, 4>{0xde, 0xad, 0xbe, 0xef}));
    secureRandomBytes(std::span<std::uint8_t>{});
}

void fillsTheWholeBufferAndNothingBeyondIt() {
    // Sentinels on both sides catch an off-by-one in the fill loop.
    std::array<std::uint8_t, 70> storage{};
    storage.fill(0xa5);
    secureRandomBytes(std::span<std::uint8_t>(storage.data() + 3, 64));

    assert(storage[0] == 0xa5 && storage[1] == 0xa5 && storage[2] == 0xa5);
    assert(storage[67] == 0xa5 && storage[68] == 0xa5 && storage[69] == 0xa5);

    // 64 random bytes all equal to the 0xa5 fill, or all zero, would be a
    // 2^-512 event; this detects "buffer left untouched".
    std::set<std::uint8_t> distinct(storage.begin() + 3, storage.begin() + 67);
    assert(distinct.size() > 1);
}

void successiveDrawsAreDistinct() {
    // Regression: on devkitA64 constructing a new std::random_device per
    // value replayed one fixed sequence, so the machine, node and disco
    // private keys came out identical. Every draw must differ.
    constexpr std::size_t kDraws = 64;
    std::set<Key> seen;
    for (std::size_t i = 0; i < kDraws; ++i)
        seen.insert(draw());
    assert(seen.size() == kDraws);
}

void keysAreNotAllZeroOrRepeatedBytes() {
    for (int i = 0; i < 32; ++i) {
        const Key key = draw();
        std::set<std::uint8_t> distinct(key.begin(), key.end());
        // 32 random bytes with at most 3 distinct values is vanishingly rare;
        // a constant or tiny-period generator hits it every time.
        assert(distinct.size() > 3);
    }
}

void byteValuesCoverTheRange() {
    std::vector<std::uint8_t> bytes(8192);
    secureRandomBytes(bytes);
    std::set<std::uint8_t> distinct(bytes.begin(), bytes.end());
    // Expected distinct values for 8192 uniform draws is ~256; requiring 200
    // leaves astronomical headroom while rejecting low-entropy output such as
    // a generator that only fills the low bits.
    assert(distinct.size() >= 200);

    // High bit and low bit must both vary (catches truncation to 7 bits or a
    // generator that always returns even values).
    bool sawHigh = false, sawLow = false, sawOdd = false, sawEven = false;
    for (auto b : bytes) {
        sawHigh |= (b & 0x80U) != 0;
        sawLow |= (b & 0x80U) == 0;
        sawOdd |= (b & 1U) != 0;
        sawEven |= (b & 1U) == 0;
    }
    assert(sawHigh && sawLow && sawOdd && sawEven);
}

void bitsAreRoughlyBalanced() {
    std::vector<std::uint8_t> bytes(16384);
    secureRandomBytes(bytes);
    std::size_t ones = 0;
    for (auto b : bytes)
        for (unsigned bit = 0; bit < 8; ++bit)
            ones += (b >> bit) & 1U;
    const std::size_t total = bytes.size() * 8;
    // Mean 65536, sigma ~ 181; +/- 5% is more than 36 sigma of slack.
    assert(ones > total * 45 / 100 && ones < total * 55 / 100);
}

void independentThreadsGetIndependentStreams() {
    // The non-Switch implementation keeps a thread_local source; two threads
    // must not end up replaying each other's output.
    Key a{}, b{};
    std::thread t1([&] { a = draw(); });
    std::thread t2([&] { b = draw(); });
    t1.join();
    t2.join();
    assert(a != b);
    assert(a != draw());
    assert(b != draw());
}

void oddSizesWork() {
    for (std::size_t size : {1U, 2U, 7U, 31U, 33U, 255U, 1000U}) {
        std::vector<std::uint8_t> bytes(size + 2, 0x3c);
        secureRandomBytes(std::span<std::uint8_t>(bytes.data() + 1, size));
        assert(bytes.front() == 0x3c && bytes.back() == 0x3c);
    }
}

} // namespace

int main() {
    zeroLengthRequestIsANoOp();
    fillsTheWholeBufferAndNothingBeyondIt();
    successiveDrawsAreDistinct();
    keysAreNotAllZeroOrRepeatedBytes();
    byteValuesCoverTheRange();
    bitsAreRoughlyBalanced();
    independentThreadsGetIndependentStreams();
    oddSizesWork();
    return 0;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#if defined(__SWITCH__)
// libnx kernel CSPRNG (switch/kernel/random.h). Declared directly so this
// header does not drag <switch.h> into every Tailscale translation unit.
extern "C" void randomGet(void* buf, size_t len);
#else
#include <random>
#endif

namespace artemis::tailscale {

// Cryptographic random bytes for keys, nonces and handshake ephemerals.
//
// Never construct a fresh std::random_device per value on Switch: on
// devkitA64 every new instance replays the same fixed sequence, which made
// the machine, node and disco private keys identical (and predictable).
inline void secureRandomBytes(std::span<std::uint8_t> output) {
#if defined(__SWITCH__)
    randomGet(output.data(), output.size());
#else
    static thread_local std::random_device source;
    for (auto& byte : output)
        byte = static_cast<std::uint8_t>(source());
#endif
}

} // namespace artemis::tailscale

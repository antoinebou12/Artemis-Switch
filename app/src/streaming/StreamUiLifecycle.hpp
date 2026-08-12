#pragma once

#include <atomic>

namespace artemis::streaming {

inline std::atomic<bool>& streamUiClosedFlag() {
    static std::atomic<bool> flag{false};
    return flag;
}

inline void markStreamUiClosed() { streamUiClosedFlag().store(true); }

inline bool consumeStreamUiClosed() {
    return streamUiClosedFlag().exchange(false);
}

} // namespace artemis::streaming

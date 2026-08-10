#pragma once

#include <mutex>

// newlib on Switch stores socket FDs in a compacting table. Concurrent
// close() while another thread is in sendto/recvfrom can IABT (see Moonlight
// issue #254 / netbird-switch notes). Serialize socket table mutations and
// lookups used by WireGuard helper threads against Moonlight I/O.
class SocketFdLock {
  public:
    static SocketFdLock& instance() {
        static SocketFdLock lock;
        return lock;
    }

    std::unique_lock<std::mutex> guard() {
        return std::unique_lock<std::mutex>(mutex_);
    }

  private:
    std::mutex mutex_;
};

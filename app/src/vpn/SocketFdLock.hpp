#pragma once

#include <mutex>

// newlib on Switch stores socket FDs in a compacting table. Concurrent
// close() while another thread is in sendto/recvfrom can IABT (see Moonlight
// issue #254 / netbird-switch notes). This lock is meant to serialize socket
// table mutations by WireGuard helper threads against Moonlight I/O.
//
// It does NOT do that yet. Nothing on the streaming side takes it: libcurl in
// libgamestream, ENet/RTSP in moonlight-common-c, and WakeOnLanManager all call
// socket APIs directly. So today the lock only ever has one participant and
// provides no mutual exclusion. Wiring the curl and ENet paths through it is a
// prerequisite for shipping a real tunnel backend; until then, treat this as a
// placeholder rather than as protection that already exists.
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

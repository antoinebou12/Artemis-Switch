#pragma once

#include <atomic>
#include <shared_mutex>

namespace artemis::stream {

// A pointer that worker threads read while the UI thread may clear it during
// teardown. The atomic pointer alone guarantees readers never observe a torn
// or half-written value; the shared_mutex additionally lets a callback pin the
// pointer for its whole body (shared lock) while the writer takes the exclusive
// lock and clears it, so a callback never races a concurrent teardown.
template <typename T>
class ActiveSessionGuard {
  public:
    ActiveSessionGuard() = default;

    ActiveSessionGuard(const ActiveSessionGuard&) = delete;
    ActiveSessionGuard& operator=(const ActiveSessionGuard&) = delete;

    T* load() const noexcept {
        return m_ptr.load(std::memory_order_acquire);
    }

    void store(T* value) noexcept {
        m_ptr.store(value, std::memory_order_release);
    }

    // A callback acquiring the session should hold this for its whole body.
    // Between the atomic load and the dereference the writer may otherwise
    // clear the pointer, so this pins the value under a shared lock.
    class ScopedRead {
      public:
        explicit ScopedRead(const ActiveSessionGuard& guard) noexcept
            : m_lock(guard.m_mutex),
              m_ptr(guard.m_ptr.load(std::memory_order_acquire)) {}
        // Pointer this reader pinned; valid for the lifetime of this object.
        T* value() const noexcept { return m_ptr; }
        explicit operator bool() const noexcept { return m_ptr != nullptr; }

      private:
        std::shared_lock<std::shared_mutex> m_lock;
        T* m_ptr;
    };

    // Acquire a shared (read) lock and snapshot the current pointer. Callers
    // that dereference the result must keep this object alive for the whole
    // body so the writer's exclusive clear cannot race their use.
    ScopedRead scopedRead() const noexcept { return ScopedRead(*this); }

  private:
    std::atomic<T*> m_ptr{nullptr};
    mutable std::shared_mutex m_mutex;
};

} // namespace artemis::stream
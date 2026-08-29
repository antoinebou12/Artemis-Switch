#pragma once

#include <atomic>
#include <memory>

// Small copyable cancellation handle for blocking GameStream HTTP operations.
// Copies share one atomic flag, so the UI can cancel without owning or closing
// the worker's CURL handle from another thread.
class CancellationToken {
public:
    CancellationToken()
        : state_(std::make_shared<std::atomic<State>>(State::Active)) {}

    // Returns true only when cancellation wins the race with completion.
    [[nodiscard]] bool cancel() const noexcept {
        auto expected = State::Active;
        return state_->compare_exchange_strong(expected, State::Cancelled);
    }

    // Called by the worker before publishing its result. Once completion wins,
    // a late UI button press cannot turn a successful pairing into a partial
    // local cancellation.
    [[nodiscard]] bool tryComplete() const noexcept {
        auto expected = State::Active;
        return state_->compare_exchange_strong(expected, State::Completed);
    }

    [[nodiscard]] bool isCancellationRequested() const noexcept {
        return state_->load() == State::Cancelled;
    }

private:
    enum class State : unsigned char { Active, Cancelled, Completed };
    std::shared_ptr<std::atomic<State>> state_;
};

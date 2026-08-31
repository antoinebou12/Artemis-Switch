#include "../app/src/libgamestream/CancellationToken.hpp"

#include <cassert>
#include <thread>

int main() {
    CancellationToken uiToken;
    // This copy is the behavior under test: UI and worker handles must share state.
    const CancellationToken workerToken = uiToken; // NOLINT(performance-unnecessary-copy-initialization)
    assert(!uiToken.isCancellationRequested());
    assert(!workerToken.isCancellationRequested());

    std::thread cancelThread([uiToken] { (void)uiToken.cancel(); });
    cancelThread.join();
    assert(uiToken.isCancellationRequested());
    assert(workerToken.isCancellationRequested());

    const CancellationToken independent;
    assert(!independent.isCancellationRequested());
    assert(independent.tryComplete());
    assert(!independent.cancel());
    assert(!independent.isCancellationRequested());

    const CancellationToken cancelledFirst;
    assert(cancelledFirst.cancel());
    assert(!cancelledFirst.tryComplete());
    return 0;
}

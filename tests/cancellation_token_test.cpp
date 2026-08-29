#include "../app/src/libgamestream/CancellationToken.hpp"

#include <cassert>
#include <thread>

int main() {
    CancellationToken uiToken;
    const CancellationToken workerToken = uiToken;
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

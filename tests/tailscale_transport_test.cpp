// These tests report failures through assert(). Keep them live in every
// build type (Release defines NDEBUG), including asserts with side effects.
#undef NDEBUG

#include "TailscaleTransport.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define ARTEMIS_TRANSPORT_LOOPBACK_TESTS 1
#endif

using namespace artemis::tailscale;
using namespace std::chrono_literals;

namespace {

std::span<const std::uint8_t> view(const std::string& text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

// ---------------------------------------------------------------------------
// Portable behaviour: the ITransport seam and TcpTransport's guard rails.
// ---------------------------------------------------------------------------

class RecordingTransport final : public ITransport {
public:
    bool connect(std::string_view, std::uint16_t, std::string*) override {
        connected = true;
        return true;
    }
    int read(std::uint8_t*, std::size_t, std::string*) override { return 0; }
    bool write(std::span<const std::uint8_t> bytes, std::string*) override {
        written += bytes.size();
        return true;
    }
    void close() noexcept override { connected = false; }

    bool connected = false;
    std::size_t written = 0;
};

void defaultInterruptIsANoOp() {
    // ITransport::interrupt() is documented as a no-op for fakes that never
    // block; it must not tear the transport down.
    RecordingTransport transport;
    ITransport& base = transport;
    assert(base.connect("relay.invalid", 443, nullptr));
    base.interrupt();
    assert(transport.connected);
    assert(base.write(view("x"), nullptr));
    assert(transport.written == 1);
}

void unconnectedTransportRejectsIo() {
    TcpTransport transport;
    std::string error;
    std::uint8_t buffer[8];

    assert(transport.read(buffer, sizeof(buffer), &error) == -1);
    assert(error == "invalid Tailscale transport read");

    error.clear();
    assert(!transport.write(view("payload"), &error));
    assert(error == "Tailscale transport is not connected");

    // An empty write on a dead transport is still an error, not a success.
    error.clear();
    assert(!transport.write({}, &error));
    assert(!error.empty());
}

void nullErrorPointerIsTolerated() {
    TcpTransport transport;
    std::uint8_t buffer[4];
    assert(transport.read(buffer, sizeof(buffer), nullptr) == -1);
    assert(!transport.write(view("x"), nullptr));
}

void closeAndInterruptAreSafeWhenNeverConnected() {
    TcpTransport transport;
    transport.interrupt();
    transport.close();
    transport.close();
    transport.interrupt();
}

#if defined(ARTEMIS_TRANSPORT_LOOPBACK_TESTS)

// ---------------------------------------------------------------------------
// Loopback socket behaviour (POSIX hosts only).
// ---------------------------------------------------------------------------

constexpr int kIoTimeoutMs = 5000;

struct Listener {
    int fd = -1;
    std::uint16_t port = 0;

    Listener() {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        assert(::bind(fd, reinterpret_cast<sockaddr*>(&address),
                      sizeof(address)) == 0);
        assert(::listen(fd, 4) == 0);
        socklen_t length = sizeof(address);
        assert(::getsockname(fd, reinterpret_cast<sockaddr*>(&address),
                             &length) == 0);
        port = ntohs(address.sin_port);
    }

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    ~Listener() { closeNow(); }

    void closeNow() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    // Accepts one connection or fails the test after a timeout instead of
    // hanging CI forever.
    int acceptOne() const {
        pollfd waiting{fd, POLLIN, 0};
        const int ready = ::poll(&waiting, 1, kIoTimeoutMs);
        assert(ready == 1 && "no client connected in time");
        const int client = ::accept(fd, nullptr, nullptr);
        assert(client >= 0);
        timeval timeout{kIoTimeoutMs / 1000, 0};
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                     sizeof(timeout));
        ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                     sizeof(timeout));
        return client;
    }
};

std::string recvExact(int fd, std::size_t count) {
    std::string out;
    char chunk[256];
    while (out.size() < count) {
        const ssize_t got =
            ::recv(fd, chunk, std::min(sizeof(chunk), count - out.size()), 0);
        if (got <= 0)
            break;
        out.append(chunk, static_cast<std::size_t>(got));
    }
    return out;
}

// True only for an orderly EOF; a 5 s receive timeout or an error is not EOF.
bool peerSawEof(int fd) {
    char byte;
    return ::recv(fd, &byte, 1, 0) == 0;
}

std::uint64_t fnv1a(std::uint64_t hash, const std::uint8_t* data,
                    std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;

void echoRoundTripAndOrderlyClose() {
    Listener listener;
    std::thread server([&] {
        const int client = listener.acceptOne();
        assert(recvExact(client, 5) == "hello");
        const std::string reply = "world!";
        assert(::send(client, reply.data(), reply.size(), 0) ==
               static_cast<ssize_t>(reply.size()));
        ::shutdown(client, SHUT_WR);
        ::close(client);
    });

    TcpTransport transport;
    std::string error;
    assert(transport.connect("127.0.0.1", listener.port, &error));
    assert(error.empty());
    assert(transport.write(view("hello"), &error));

    std::string received;
    std::uint8_t buffer[16];
    for (;;) {
        const int count = transport.read(buffer, sizeof(buffer), &error);
        assert(count >= 0);
        if (count == 0)
            break; // orderly close by the peer
        received.append(reinterpret_cast<const char*>(buffer),
                        static_cast<std::size_t>(count));
    }
    assert(received == "world!");

    // End of stream stays end of stream.
    assert(transport.read(buffer, sizeof(buffer), &error) == 0);
    server.join();
}

void connectToClosedPortFailsAndLeavesTransportUnusable() {
    std::uint16_t deadPort = 0;
    {
        Listener listener;
        deadPort = listener.port;
    } // listener closed: nothing is accepting on this port any more

    TcpTransport transport;
    std::string error;
    assert(!transport.connect("127.0.0.1", deadPort, &error));
    assert(error.rfind("cannot connect to Tailscale relay (", 0) == 0);

    error.clear();
    assert(!transport.write(view("x"), &error));
    assert(error == "Tailscale transport is not connected");
}

void readRejectsNullBufferAndZeroLengthOnLiveConnection() {
    Listener listener;
    TcpTransport transport;
    assert(transport.connect("127.0.0.1", listener.port, nullptr));
    const int peer = listener.acceptOne();

    std::string error;
    std::uint8_t buffer[4];
    assert(transport.read(nullptr, 4, &error) == -1);
    assert(error == "invalid Tailscale transport read");
    error.clear();
    assert(transport.read(buffer, 0, &error) == -1);
    assert(error == "invalid Tailscale transport read");

    ::close(peer);
}

void largeWriteIsDeliveredIntact() {
    // Far larger than any socket buffer: every byte must arrive, in order,
    // while the peer drains concurrently. (A blocking send() normally takes the
    // whole buffer in one call, so this does not force the partial-send retry
    // in TcpTransport::write(); that loop only matters under signals or
    // timeouts, which a loopback test cannot provoke without reaching into the
    // transport's private socket.)
    constexpr std::size_t kTotal = 4U * 1024U * 1024U;
    std::vector<std::uint8_t> payload(kTotal);
    for (std::size_t i = 0; i < kTotal; ++i)
        payload[i] = static_cast<std::uint8_t>(i * 31U + 7U);
    const std::uint64_t expectedHash =
        fnv1a(kFnvOffset, payload.data(), payload.size());

    Listener listener;
    std::atomic<std::size_t> receivedBytes{0};
    std::atomic<std::uint64_t> receivedHash{0};
    std::thread server([&] {
        const int client = listener.acceptOne();
        std::uint64_t hash = kFnvOffset;
        std::size_t total = 0;
        std::vector<std::uint8_t> chunk(64 * 1024);
        for (;;) {
            const ssize_t got = ::recv(client, chunk.data(), chunk.size(), 0);
            if (got <= 0)
                break;
            hash = fnv1a(hash, chunk.data(), static_cast<std::size_t>(got));
            total += static_cast<std::size_t>(got);
        }
        receivedBytes = total;
        receivedHash = hash;
        ::close(client);
    });

    TcpTransport transport;
    std::string error;
    assert(transport.connect("127.0.0.1", listener.port, &error));
    assert(transport.write(payload, &error));
    transport.close();
    server.join();

    assert(receivedBytes == kTotal);
    assert(receivedHash == expectedHash);
}

void interruptWakesABlockedRead() {
    Listener listener;
    TcpTransport transport;
    assert(transport.connect("127.0.0.1", listener.port, nullptr));
    const int peer = listener.acceptOne(); // peer stays silent on purpose

    std::atomic<bool> done{false};
    std::atomic<int> result{12345};
    std::thread reader([&] {
        std::uint8_t buffer[8];
        std::string error;
        result = transport.read(buffer, sizeof(buffer), &error);
        done = true;
    });

    std::this_thread::sleep_for(150ms);
    assert(!done && "read returned before interrupt()");

    transport.interrupt(); // documented as safe from another thread

    bool finished = false;
    for (int i = 0; i < 500 && !finished; ++i) {
        finished = done;
        if (!finished)
            std::this_thread::sleep_for(10ms);
    }
    if (!finished)
        ::shutdown(peer, SHUT_RDWR); // free the reader so join() cannot hang
    reader.join();

    assert(finished && "interrupt() did not wake a blocked read");
    assert(result <= 0); // EOF or error, never data

    ::close(peer);
    transport.close(); // the I/O thread still owns the final close
}

void reconnectReplacesThePreviousConnection() {
    Listener first;
    Listener second;
    TcpTransport transport;

    assert(transport.connect("127.0.0.1", first.port, nullptr));
    const int firstPeer = first.acceptOne();

    // connect() on a live transport must drop the old socket, not leak it.
    assert(transport.connect("127.0.0.1", second.port, nullptr));
    const int secondPeer = second.acceptOne();

    assert(peerSawEof(firstPeer)); // old peer sees an orderly EOF

    assert(::send(secondPeer, "ok", 2, 0) == 2);
    std::uint8_t buffer[2] = {0, 0};
    std::string error;
    int total = 0;
    while (total < 2) {
        const int count = transport.read(buffer + total,
                                         static_cast<std::size_t>(2 - total),
                                         &error);
        assert(count > 0);
        total += count;
    }
    assert(buffer[0] == 'o' && buffer[1] == 'k');

    ::close(firstPeer);
    ::close(secondPeer);
}

void writeAfterCloseFailsAndCloseIsIdempotent() {
    Listener listener;
    TcpTransport transport;
    assert(transport.connect("127.0.0.1", listener.port, nullptr));
    const int peer = listener.acceptOne();

    transport.close();
    transport.close();
    transport.interrupt(); // after close: must not touch a stale descriptor

    std::string error;
    assert(!transport.write(view("late"), &error));
    assert(error == "Tailscale transport is not connected");
    assert(peerSawEof(peer));

    ::close(peer);
}

#endif // ARTEMIS_TRANSPORT_LOOPBACK_TESTS

} // namespace

int main() {
#if defined(SIGPIPE)
    // A peer that vanishes mid-write must surface as an error return, not a
    // process-killing signal, in every test below.
    std::signal(SIGPIPE, SIG_IGN);
#endif

    defaultInterruptIsANoOp();
    unconnectedTransportRejectsIo();
    nullErrorPointerIsTolerated();
    closeAndInterruptAreSafeWhenNeverConnected();

#if defined(ARTEMIS_TRANSPORT_LOOPBACK_TESTS)
    echoRoundTripAndOrderlyClose();
    connectToClosedPortFailsAndLeavesTransportUnusable();
    readRejectsNullBufferAndZeroLengthOnLiveConnection();
    largeWriteIsDeliveredIntact();
    interruptWakesABlockedRead();
    reconnectReplacesThePreviousConnection();
    writeAfterCloseFailsAndCloseIsIdempotent();
#else
    std::cout << "loopback socket tests skipped on this platform\n";
#endif
    return 0;
}

#include "TailscaleTransport.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

// Portable POSIX-socket transport. On Switch this resolves through newlib's
// socket layer; on other hosts it is compiled only if a build opts it in.
#if defined(__SWITCH__)
extern "C" {
#include <switch.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace artemis::tailscale {
namespace {

int lastErrorNumber() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool lookup(std::string_view host, uint16_t port, sockaddr_storage* storage,
            socklen_t* length) {
    char portText[8];
    std::snprintf(portText, sizeof(portText), "%u",
                  static_cast<unsigned>(port));
    const std::string hostString(host);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const int rc = getaddrinfo(hostString.c_str(), portText, &hints, &result);
    if (rc != 0 || !result) return false;
    std::memcpy(storage, result->ai_addr, result->ai_addrlen);
    *length = static_cast<socklen_t>(result->ai_addrlen);
    freeaddrinfo(result);
    return true;
}

} // namespace

bool TcpTransport::connect(std::string_view host, uint16_t port,
                           std::string* error) {
    close();
    sockaddr_storage address{};
    socklen_t addressLength = 0;
    if (!lookup(host, port, &address, &addressLength)) {
        if (error) *error = "cannot resolve Tailscale relay host";
        return false;
    }
#if defined(_WIN32)
    socket_ = ::socket(address.ss_family, SOCK_STREAM, IPPROTO_TCP);
#else
    socket_ = static_cast<int>(
        ::socket(address.ss_family, SOCK_STREAM, 0));
#endif
    if (socket_ < 0) {
        if (error) *error = "cannot open Tailscale transport socket";
        return false;
    }
    if (::connect(socket_, reinterpret_cast<sockaddr*>(&address),
                  addressLength) != 0) {
        const auto err = lastErrorNumber();
        if (error)
            *error = "cannot connect to Tailscale relay (" +
                     std::to_string(err) + ")";
        close();
        return false;
    }
    return true;
}

int TcpTransport::read(uint8_t* buffer, size_t length, std::string* error) {
    if (socket_ < 0 || !buffer || length == 0) {
        if (error) *error = "invalid Tailscale transport read";
        return -1;
    }
#if defined(_WIN32)
    int received;
    do {
        received = ::recv(socket_, reinterpret_cast<char*>(buffer),
                          static_cast<int>(length), 0);
    } while (received < 0 && WSAGetLastError() == WSAEINTR);
    if (received == 0) return 0;
    if (received < 0) {
        if (error)
            *error = "Tailscale transport read failed (" +
                     std::to_string(WSAGetLastError()) + ")";
        return -1;
    }
    return received;
#else
    const ssize_t received = ::recv(socket_, buffer, length, 0);
    if (received == 0) return 0;
    if (received < 0) {
        if (error)
            *error = "Tailscale transport read failed (" +
                     std::to_string(lastErrorNumber()) + ")";
        return -1;
    }
    return static_cast<int>(received);
#endif
}

bool TcpTransport::write(std::span<const uint8_t> bytes, std::string* error) {
    if (socket_ < 0) {
        if (error) *error = "Tailscale transport is not connected";
        return false;
    }
    size_t offset = 0;
    while (offset < bytes.size()) {
#if defined(_WIN32)
        const auto sent = ::send(socket_,
                                 reinterpret_cast<const char*>(bytes.data() + offset),
                                 static_cast<int>(bytes.size() - offset), 0);
        if (sent < 0) {
            if (error)
                *error = "Tailscale transport write failed (" +
                         std::to_string(WSAGetLastError()) + ")";
            return false;
        }
#else
        const auto sent =
            ::send(socket_, bytes.data() + offset, bytes.size() - offset, 0);
        if (sent < 0) {
            if (error)
                *error = "Tailscale transport write failed (" +
                         std::to_string(lastErrorNumber()) + ")";
            return false;
        }
#endif
        offset += static_cast<size_t>(sent);
    }
    return true;
}

void TcpTransport::close() noexcept {
    if (socket_ >= 0) {
#if defined(_WIN32)
        ::closesocket(socket_);
        WSACleanup();
#elif defined(__SWITCH__)
        ::close(socket_);
#else
        ::close(socket_);
#endif
        socket_ = -1;
    }
}

#if defined(__SWITCH__)

struct SwitchTlsTransport::Impl {
    int socket = -1;
    bool sslInitialized = false;
    bool contextOpen = false;
    bool connectionOpen = false;
    SslContext context{};
    SslConnection connection{};
};

namespace {

std::string sslError(std::string_view operation, Result result) {
    char code[24];
    std::snprintf(code, sizeof(code), "0x%08x",
                  static_cast<unsigned>(result));
    return std::string(operation) + " failed (" + code + ")";
}

} // namespace

SwitchTlsTransport::SwitchTlsTransport() : impl_(std::make_unique<Impl>()) {}

SwitchTlsTransport::~SwitchTlsTransport() { close(); }

bool SwitchTlsTransport::connect(std::string_view host, std::uint16_t port,
                                 std::string* error) {
    close();
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (host.empty() || host.size() > 254 ||
        host.find_first_of("\r\n") != std::string_view::npos) {
        if (error) *error = "invalid Tailscale TLS hostname";
        return false;
    }

    sockaddr_storage address{};
    socklen_t addressLength = 0;
    if (!lookup(host, port, &address, &addressLength)) {
        if (error) *error = "cannot resolve Tailscale TLS host";
        return false;
    }
    impl_->socket = static_cast<int>(::socket(address.ss_family, SOCK_STREAM, 0));
    if (impl_->socket < 0 ||
        ::connect(impl_->socket, reinterpret_cast<sockaddr*>(&address),
                  addressLength) != 0) {
        if (error) *error = "cannot connect to Tailscale TLS endpoint";
        close();
        return false;
    }

    Result result = sslInitialize(3);
    if (R_FAILED(result)) {
        if (error) *error = sslError("Tailscale SSL initialization", result);
        close();
        return false;
    }
    impl_->sslInitialized = true;

    result = sslCreateContext(&impl_->context, SslVersion_TlsV12);
    if (R_FAILED(result)) {
        if (error) *error = sslError("Tailscale TLS context", result);
        close();
        return false;
    }
    impl_->contextOpen = true;
    result = sslContextCreateConnection(&impl_->context, &impl_->connection);
    if (R_FAILED(result)) {
        if (error) *error = sslError("Tailscale TLS connection", result);
        close();
        return false;
    }
    impl_->connectionOpen = true;

    result = sslConnectionSetOption(&impl_->connection,
                                    SslOptionType_DoNotCloseSocket, true);
    if (R_FAILED(result)) {
        if (error) *error = sslError("Tailscale TLS socket ownership", result);
        close();
        return false;
    }
    errno = 0;
    const int duplicate = socketSslConnectionSetSocketDescriptor(
        &impl_->connection, impl_->socket);
    if (duplicate >= 0) {
        ::close(duplicate);
    } else if (errno != ENOENT) {
        if (error) *error = "Tailscale TLS socket registration failed";
        close();
        return false;
    }

    const std::string hostname(host);
    result = sslConnectionSetHostName(
        &impl_->connection, hostname.c_str(),
        static_cast<std::uint32_t>(hostname.size() + 1));
    if (R_SUCCEEDED(result)) {
        result = sslConnectionSetVerifyOption(
            &impl_->connection, SslVerifyOption_PeerCa |
                                    SslVerifyOption_HostName |
                                    SslVerifyOption_DateCheck);
    }
    if (R_SUCCEEDED(result)) {
        result = sslConnectionDoHandshake(&impl_->connection, nullptr, nullptr,
                                          nullptr, 0);
    }
    if (R_FAILED(result)) {
        if (error) *error = sslError("verified Tailscale TLS handshake", result);
        close();
        return false;
    }
    return true;
}

int SwitchTlsTransport::read(std::uint8_t* buffer, std::size_t length,
                             std::string* error) {
    if (!impl_ || !impl_->connectionOpen || !buffer || length == 0) {
        if (error) *error = "invalid Tailscale TLS read";
        return -1;
    }
    std::uint32_t transferred = 0;
    const Result result = sslConnectionRead(
        &impl_->connection, buffer,
        static_cast<std::uint32_t>(
            std::min<std::size_t>(length, std::numeric_limits<std::uint32_t>::max())),
        &transferred);
    if (R_FAILED(result)) {
        if (error) *error = sslError("Tailscale TLS read", result);
        return -1;
    }
    return static_cast<int>(transferred);
}

bool SwitchTlsTransport::write(std::span<const std::uint8_t> bytes,
                               std::string* error) {
    if (!impl_ || !impl_->connectionOpen) {
        if (error) *error = "Tailscale TLS transport is not connected";
        return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        std::uint32_t transferred = 0;
        const auto amount = static_cast<std::uint32_t>(std::min<std::size_t>(
            bytes.size() - offset,
            std::numeric_limits<std::uint32_t>::max()));
        const Result result = sslConnectionWrite(
            &impl_->connection, bytes.data() + offset, amount, &transferred);
        if (R_FAILED(result) || transferred == 0) {
            if (error)
                *error = R_FAILED(result)
                             ? sslError("Tailscale TLS write", result)
                             : "Tailscale TLS write made no progress";
            return false;
        }
        offset += transferred;
    }
    return true;
}

void SwitchTlsTransport::close() noexcept {
    if (!impl_) return;
    if (impl_->connectionOpen) {
        sslConnectionClose(&impl_->connection);
        impl_->connectionOpen = false;
    }
    if (impl_->contextOpen) {
        sslContextClose(&impl_->context);
        impl_->contextOpen = false;
    }
    if (impl_->sslInitialized) {
        sslExit();
        impl_->sslInitialized = false;
    }
    if (impl_->socket >= 0) {
        ::close(impl_->socket);
        impl_->socket = -1;
    }
}

#endif

} // namespace artemis::tailscale

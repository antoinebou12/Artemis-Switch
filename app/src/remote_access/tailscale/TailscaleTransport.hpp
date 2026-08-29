#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace artemis::tailscale {

// Injectable, portable byte-stream seam. Nothing in the Tailscale protocol or
// session layers owns socket state; a concrete transport is supplied by the
// platform adapter so every protocol path is unit-testable on the host without
// real network I/O.
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool connect(std::string_view host, std::uint16_t port,
                         std::string* error) = 0;
    // Reads up to `length` bytes. Returns the number of bytes read, 0 on an
    // orderly close, or -1 on error (see *error).
    virtual int read(std::uint8_t* buffer, std::size_t length,
                     std::string* error) = 0;
    // Writes all bytes. Returns true on success.
    virtual bool write(std::span<const std::uint8_t> bytes,
                       std::string* error) = 0;
    virtual void close() noexcept = 0;
};

// Blocking TCP transport over the POSIX socket API so it is also usable on a
// std::thread worker. On non-POSIX targets it reports that it is unsupported.
class TcpTransport final : public ITransport {
public:
    TcpTransport() = default;
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    ~TcpTransport() override { close(); }

    bool connect(std::string_view host, std::uint16_t port,
                 std::string* error) override;
    int read(std::uint8_t* buffer, std::size_t length,
             std::string* error) override;
    bool write(std::span<const std::uint8_t> bytes,
               std::string* error) override;
    void close() noexcept override;

private:
    int socket_ = -1;
};

#if defined(__SWITCH__)
// Verified TLS using Horizon's SSL service and built-in CA store. This is the
// outer HTTPS transport only; TS2021 Noise and HTTP/2 remain separate layers.
class SwitchTlsTransport final : public ITransport {
public:
    SwitchTlsTransport();
    SwitchTlsTransport(const SwitchTlsTransport&) = delete;
    SwitchTlsTransport& operator=(const SwitchTlsTransport&) = delete;
    ~SwitchTlsTransport() override;

    bool connect(std::string_view host, std::uint16_t port,
                 std::string* error) override;
    int read(std::uint8_t* buffer, std::size_t length,
             std::string* error) override;
    bool write(std::span<const std::uint8_t> bytes,
               std::string* error) override;
    void close() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif

} // namespace artemis::tailscale

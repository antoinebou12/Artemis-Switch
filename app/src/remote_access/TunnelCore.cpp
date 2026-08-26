#include "TunnelCore.hpp"
#include <borealis/core/logger.hpp>

TunnelCore& TunnelCore::instance() {
    static TunnelCore inst;
    return inst;
}

bool TunnelCore::init() {
    if (initialized_) return true;
    brls::Logger::info("TunnelCore: init");
    initialized_ = true;
    return true;
}

bool TunnelCore::start() {
    if (!initialized_) return false;
    brls::Logger::info("TunnelCore: start");
    return true;
}

void TunnelCore::poll() {}

void TunnelCore::stop() {
    brls::Logger::info("TunnelCore: stop");
}

bool TunnelCore::addPeer(const WireGuardPeer& peer) {
    brls::Logger::info("TunnelCore: add peer {}", peer.publicKey);
    return true;
}

bool TunnelCore::setInterface(const std::string& privateKey, const std::string& address) {
    brls::Logger::info("TunnelCore: set interface {}", address);
    return true;
}

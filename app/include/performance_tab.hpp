#pragma once

#include <borealis.hpp>

class PerformanceTab : public brls::Box {
public:
    PerformanceTab();

    static brls::View* create() { return new PerformanceTab(); }

private:
    void refresh();

    BRLS_BIND(brls::DetailCell, streamProfile, "stream_profile");
    BRLS_BIND(brls::DetailCell, network, "network");
    BRLS_BIND(brls::DetailCell, receiveLatency, "receive_latency");
    BRLS_BIND(brls::DetailCell, decodeLatency, "decode_latency");
    BRLS_BIND(brls::DetailCell, renderLatency, "render_latency");
    BRLS_BIND(brls::DetailCell, packetLoss, "packet_loss");
    BRLS_BIND(brls::DetailCell, renderedFps, "rendered_fps");
    BRLS_BIND(brls::DetailCell, queueDepth, "queue_depth");
    BRLS_BIND(brls::DetailCell, refreshButton, "refresh");

    uint64_t previousReceived = 0;
    uint64_t previousDropped = 0;
};

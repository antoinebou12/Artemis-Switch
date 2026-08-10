#pragma once

#include <borealis.hpp>

class PerformanceTab : public brls::Box {
public:
    PerformanceTab();

    static brls::View* create() { return new PerformanceTab(); }

private:
    void refresh();
    void updateBenchmarkStatus();
    void updateAutoTuneStatus();

    BRLS_BIND(brls::DetailCell, streamProfile, "stream_profile");
    BRLS_BIND(brls::DetailCell, network, "network");
    BRLS_BIND(brls::DetailCell, receiveLatency, "receive_latency");
    BRLS_BIND(brls::DetailCell, decodeLatency, "decode_latency");
    BRLS_BIND(brls::DetailCell, renderLatency, "render_latency");
    BRLS_BIND(brls::DetailCell, packetLoss, "packet_loss");
    BRLS_BIND(brls::DetailCell, renderedFps, "rendered_fps");
    BRLS_BIND(brls::DetailCell, queueDepth, "queue_depth");
    BRLS_BIND(brls::DetailCell, benchmarkSummary, "benchmark_summary");
    BRLS_BIND(brls::DetailCell, benchmarkAction, "benchmark_action");
    BRLS_BIND(brls::DetailCell, benchmarkSave, "benchmark_save");
    BRLS_BIND(brls::DetailCell, benchmarkReset, "benchmark_reset");
    BRLS_BIND(brls::DetailCell, autoTuneSummary, "auto_tune_summary");
    BRLS_BIND(brls::DetailCell, autoTuneAction, "auto_tune_action");
    BRLS_BIND(brls::DetailCell, autoTuneAdvanced, "auto_tune_advanced");
    BRLS_BIND(brls::DetailCell, refreshButton, "refresh");

    uint64_t previousReceived = 0;
    uint64_t previousDropped = 0;
};

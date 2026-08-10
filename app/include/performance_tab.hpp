#pragma once

#include <borealis.hpp>

#include "wifi_performance_graph.hpp"

class PerformanceTab : public brls::Box {
public:
    PerformanceTab();
    ~PerformanceTab() override;

    static brls::View* create() { return new PerformanceTab(); }

private:
    void refresh();
    void updateBenchmarkStatus();
    void scheduleRefresh();

    BRLS_BIND(brls::DetailCell, network, "network");
    BRLS_BIND(brls::DetailCell, wifiSignal, "wifi_signal");
    BRLS_BIND(WifiPerformanceGraph, wifiGraph, "wifi_graph");
    BRLS_BIND(brls::DetailCell, receiveLatency, "receive_latency");
    BRLS_BIND(brls::DetailCell, packetLoss, "packet_loss");
    BRLS_BIND(brls::DetailCell, renderedFps, "rendered_fps");
    BRLS_BIND(brls::DetailCell, benchmarkSummary, "benchmark_summary");
    BRLS_BIND(brls::DetailCell, benchmarkAction, "benchmark_action");
    BRLS_BIND(brls::DetailCell, benchmarkSave, "benchmark_save");
    BRLS_BIND(brls::DetailCell, benchmarkReset, "benchmark_reset");
    uint64_t previousReceived = 0;
    uint64_t previousDropped = 0;
    size_t refreshTask = 0;
    unsigned wifiQueryTicks = 1;
};

#pragma once

#include <borealis.hpp>

#include "wifi_performance_graph.hpp"

class StreamingView;

class PerformanceTab : public brls::Box {
public:
    explicit PerformanceTab(StreamingView* streamView = nullptr);
    ~PerformanceTab() override;

    static brls::View* create() { return new PerformanceTab(nullptr); }

private:
    void refresh();
    void updateBenchmarkStatus();
    void scheduleRefresh();

    StreamingView* streamView = nullptr;

    BRLS_BIND(brls::DetailCell, network, "network");
    BRLS_BIND(brls::DetailCell, wifiSignal, "wifi_signal");
    BRLS_BIND(WifiPerformanceGraph, wifiGraph, "wifi_graph");
    BRLS_BIND(brls::DetailCell, receiveLatency, "receive_latency");
    BRLS_BIND(brls::DetailCell, decodeLatency, "decode_latency");
    BRLS_BIND(brls::DetailCell, renderLatency, "render_latency");
    BRLS_BIND(brls::DetailCell, packetLoss, "packet_loss");
    BRLS_BIND(brls::DetailCell, hostFps, "host_fps");
    BRLS_BIND(brls::DetailCell, receivedFps, "received_fps");
    BRLS_BIND(WifiPerformanceGraph, receivedFpsGraph, "received_fps_graph");
    BRLS_BIND(brls::DetailCell, decodedFps, "decoded_fps");
    BRLS_BIND(WifiPerformanceGraph, decodedFpsGraph, "decoded_fps_graph");
    BRLS_BIND(brls::DetailCell, renderedFps, "rendered_fps");
    BRLS_BIND(brls::DetailCell, frameQueue, "frame_queue");
    BRLS_BIND(brls::DetailCell, presentation, "presentation");
    BRLS_BIND(brls::DetailCell, operationMode, "operation_mode");
    BRLS_BIND(brls::DetailCell, cpuClock, "cpu_clock");
    BRLS_BIND(brls::DetailCell, gpuClock, "gpu_clock");
    BRLS_BIND(brls::DetailCell, memoryClock, "memory_clock");
    BRLS_BIND(brls::DetailCell, battery, "battery");
    BRLS_BIND(brls::DetailCell, benchmarkSummary, "benchmark_summary");
    BRLS_BIND(brls::DetailCell, benchmarkAction, "benchmark_action");
    BRLS_BIND(brls::DetailCell, benchmarkSave, "benchmark_save");
    BRLS_BIND(brls::DetailCell, benchmarkReset, "benchmark_reset");
    BRLS_BIND(brls::BooleanCell, debugButton, "debug");
    BRLS_BIND(brls::BooleanCell, onscreenLogButton, "onscreen_log");
    uint64_t previousReceived = 0;
    uint64_t previousDropped = 0;
    size_t refreshTask = 0;
    unsigned wifiQueryTicks = 1;
};

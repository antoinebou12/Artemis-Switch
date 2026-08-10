#include "performance_tab.hpp"

#include "AVFrameHolder.hpp"
#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include "features/performance/PerformanceLite.hpp"

#if __has_include("benchmark/BenchmarkRuntime.hpp")
#include "benchmark/BenchmarkRuntime.hpp"
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 1
#else
#define ARTEMIS_HAS_BENCHMARK_RUNTIME 0
#endif

#if ARTEMIS_HAS_BENCHMARK_RUNTIME && __has_include("benchmark/BenchmarkFileStore.hpp")
#include "benchmark/BenchmarkFileStore.hpp"
#define ARTEMIS_HAS_BENCHMARK_EXPORT 1
#else
#define ARTEMIS_HAS_BENCHMARK_EXPORT 0
#endif

#if __has_include("benchmark/AutoTuneRuntime.hpp")
#include "benchmark/AutoTuneRuntime.hpp"
#define ARTEMIS_HAS_AUTO_TUNE 1
#else
#define ARTEMIS_HAS_AUTO_TUNE 0
#endif

#if __has_include("streaming/StreamProfileStore.hpp")
#include "streaming/StreamProfileStore.hpp"
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 1
#else
#define ARTEMIS_HAS_CUSTOM_STREAM_PROFILE 0
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <utility>

using namespace brls;

namespace {
std::pair<int, int> configuredDimensions() {
#if ARTEMIS_HAS_CUSTOM_STREAM_PROFILE
    const auto custom = artemis::streaming::StreamProfileStore::instance().get();
    if (custom.customResolutionEnabled)
        return {custom.width, custom.height};
#endif

    const int resolution = Settings::instance().resolution();
    if (resolution == -1)
        return {Application::windowWidth, Application::windowHeight};
    return {resolution * 16 / 9, resolution};
}

#if ARTEMIS_HAS_BENCHMARK_EXPORT
artemis::benchmark::ExportProfile exportProfile() {
    const auto [width, height] = configuredDimensions();
    artemis::benchmark::ExportProfile profile;
    profile.width = width;
    profile.height = height;
    profile.fps = Settings::instance().fps();
    profile.bitrateKbps = Settings::instance().bitrate();
    profile.decoderThreads = Settings::instance().decoder_threads();
    profile.codec = Settings::instance().video_codec() == H264 ? "H264" : "HEVC";
    return profile;
}

artemis::benchmark::ExportSummary exportSummary(
    const artemis::benchmark::BenchmarkSummary& summary) {
    artemis::benchmark::ExportSummary result;
    result.durationSeconds = summary.durationSeconds;
    result.renderedFpsMean = summary.renderedFps.mean;
    result.renderedFpsP99 = summary.renderedFps.p99;
    result.networkDropPercent = summary.networkDropPercent;
    result.receiveMsP99 = summary.receiveMs.p99;
    result.decodeMsP99 = summary.decodeMs.p99;
    result.clientProcessingMsP99 = summary.clientProcessingMs.p99;
    result.stabilityScore = summary.stabilityScore;
    return result;
}
#endif
}

PerformanceTab::PerformanceTab() {
    inflateFromXMLRes("xml/views/ingame_overlay/performance_tab.xml");

    const std::array<DetailCell*, 15> compactRows = {
        network, receiveLatency, decodeLatency, renderLatency,
        packetLoss, renderedFps, queueDepth, benchmarkSummary,
        benchmarkAction, benchmarkSave, benchmarkReset, autoTuneSummary,
        autoTuneAction, autoTuneAdvanced, refreshButton};
    for (auto* row : compactRows) {
        row->title->setSingleLine(true);
        row->detail->setSingleLine(true);
    }

    // Auto Tune results can still be wider than the Switch overlay. Keep the
    // status in one clipped column and continuously marquee it in accent blue.
    autoTuneSummary->detail->setHorizontalAlign(HorizontalAlign::LEFT);
    autoTuneSummary->detail->setTextColor(
        Application::getTheme()["brls/accent"]);
    autoTuneSummary->detail->setAutoAnimate(false);
    autoTuneSummary->detail->setAnimated(true);

    network->setText("artemis/performance/live_bitrate"_i18n);
    network->registerClickAction([this](View*) {
        auto* session = MoonlightSession::activeSession();
        if (!session || !session->is_active())
            return true;

        const int currentMbps =
            std::max(1, Settings::instance().bitrate() / 1000);
        Application::getImeManager()->openForNumber(
            [this](long number) {
                const int mbps =
                    std::clamp(static_cast<int>(number), 1, 100);
                auto* active = MoonlightSession::activeSession();
                if (!active || !active->is_active()) {
                    network->setDetailText(
                        "artemis/performance/no_active_stream"_i18n);
                    return;
                }

                bitrateRestartPending =
                    active->applyBitrateKbps(mbps * 1000);
                network->setDetailText(
                    bitrateRestartPending
                        ? "artemis/performance/reconnecting_bitrate"_i18n
                        : fmt::format("{:.1f} Mbps",
                                      static_cast<double>(mbps)));
            },
            "artemis/performance/live_bitrate_title"_i18n,
            "artemis/performance/live_bitrate_hint"_i18n, 3,
            std::to_string(currentMbps), "", "", 0);
        return true;
    });
    receiveLatency->setText("artemis/performance/receive_latency"_i18n);
    decodeLatency->setText("artemis/performance/decode_latency"_i18n);
    renderLatency->setText("artemis/performance/render_latency"_i18n);
    packetLoss->setText("artemis/performance/packet_loss"_i18n);
    renderedFps->setText("artemis/performance/rendered_fps"_i18n);
    queueDepth->setText("artemis/performance/frame_queue"_i18n);
    benchmarkSummary->setText("artemis/performance/benchmark"_i18n);
    benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
    benchmarkSave->setText("artemis/performance/save_benchmark"_i18n);
    benchmarkReset->setText("artemis/performance/reset_benchmark"_i18n);
    autoTuneSummary->setText("artemis/performance/auto_tune_status"_i18n);
    autoTuneAction->setText("artemis/performance/quick_auto_tune"_i18n);
    autoTuneAdvanced->setText("artemis/performance/advanced_auto_tune"_i18n);
    refreshButton->setText("artemis/performance/refresh"_i18n);

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkSummary->setDetailText("artemis/performance/ready"_i18n);
    benchmarkAction->registerClickAction([this](View*) {
#if ARTEMIS_HAS_AUTO_TUNE
        if (artemis::benchmark::AutoTuneRuntime::instance().running())
            return true;
#endif
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        if (runtime.running()) {
            const auto result = runtime.stop();
            benchmarkSummary->setDetailText(fmt::format(
                "{:.0f}/100 · {:.2f}% · {:.1f} ms",
                result.stabilityScore, result.networkDropPercent,
                result.clientProcessingMs.p99));
        } else {
            runtime.start(Settings::instance().fps());
        }
        updateBenchmarkStatus();
        updateAutoTuneStatus();
        return true;
    });
    benchmarkReset->registerClickAction([this](View*) {
#if ARTEMIS_HAS_AUTO_TUNE
        if (artemis::benchmark::AutoTuneRuntime::instance().running())
            return true;
#endif
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        runtime.reset();
        benchmarkSummary->setDetailText(
            "artemis/performance/reset_done"_i18n);
        updateBenchmarkStatus();
        return true;
    });
#else
    benchmarkSummary->setDetailText("artemis/performance/benchmark_unavailable"_i18n);
    benchmarkAction->setFocusable(false);
    benchmarkReset->setFocusable(false);
#endif

#if ARTEMIS_HAS_BENCHMARK_EXPORT
    benchmarkSave->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
        const auto summary = runtime.snapshot();
        if (runtime.running() || summary.sampleCount < 2) {
            benchmarkSave->setDetailText("artemis/performance/stop_collect_first"_i18n);
            return true;
        }

        const auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto directory = std::filesystem::path(Settings::instance().working_dir()) /
                               "benchmarks";
        const auto paths = artemis::benchmark::BenchmarkFileStore::save(
            directory, fmt::format("benchmark_{}", epoch), exportProfile(),
            exportSummary(summary));
        benchmarkSave->setDetailText(paths
            ? paths->json.filename().string()
            : "artemis/performance/save_failed"_i18n);
        return true;
    });
#else
    benchmarkSave->setDetailText("artemis/performance/export_unavailable"_i18n);
    benchmarkSave->setFocusable(false);
#endif

#if ARTEMIS_HAS_AUTO_TUNE
    autoTuneAction->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::AutoTuneRuntime::instance();
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
        if (!runtime.running() &&
            artemis::benchmark::BenchmarkRuntime::instance().running())
            return true;
#endif
        if (runtime.running())
            runtime.cancel();
        else
            runtime.start(false);
        updateAutoTuneStatus();
        updateBenchmarkStatus();
        return true;
    });
    autoTuneAdvanced->registerClickAction([this](View*) {
        auto& runtime = artemis::benchmark::AutoTuneRuntime::instance();
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
        if (!runtime.running() &&
            artemis::benchmark::BenchmarkRuntime::instance().running())
            return true;
#endif
        if (runtime.running())
            runtime.cancel();
        else
            runtime.start(true);
        updateAutoTuneStatus();
        updateBenchmarkStatus();
        return true;
    });
#else
    autoTuneSummary->setDetailText("artemis/performance/auto_tune_unavailable"_i18n);
    autoTuneAction->setFocusable(false);
    autoTuneAdvanced->setFocusable(false);
#endif

    refreshButton->registerClickAction([this](View*) {
        refresh();
        return true;
    });

    refresh();
    scheduleRefresh();
}

PerformanceTab::~PerformanceTab() {
    if (refreshTask != 0)
        cancelDelay(refreshTask);
}

void PerformanceTab::scheduleRefresh() {
    refreshTask = delay(500, [this] {
        refreshTask = 0;
        refresh();
        scheduleRefresh();
    });
}

void PerformanceTab::updateBenchmarkStatus() {
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    auto& runtime = artemis::benchmark::BenchmarkRuntime::instance();
#if ARTEMIS_HAS_AUTO_TUNE
    const bool tuneRunning =
        artemis::benchmark::AutoTuneRuntime::instance().running();
#else
    const bool tuneRunning = false;
#endif
    if (runtime.running()) {
        const auto summary = runtime.snapshot();
        benchmarkAction->setText("artemis/performance/stop_benchmark"_i18n);
        benchmarkAction->setFocusable(!tuneRunning);
        benchmarkSummary->setDetailText(fmt::format(
            "{} · {:.0f}/100",
            runtime.sampleCount(), summary.stabilityScore));
        benchmarkSave->setFocusable(false);
        benchmarkReset->setFocusable(false);
    } else {
        benchmarkAction->setText("artemis/performance/start_benchmark"_i18n);
        benchmarkAction->setFocusable(!tuneRunning);
#if ARTEMIS_HAS_BENCHMARK_EXPORT
        benchmarkSave->setFocusable(!tuneRunning && runtime.sampleCount() >= 2);
#endif
        benchmarkReset->setFocusable(!tuneRunning && runtime.sampleCount() > 0);
    }
#endif
}

void PerformanceTab::updateAutoTuneStatus() {
#if ARTEMIS_HAS_AUTO_TUNE
    auto& runtime = artemis::benchmark::AutoTuneRuntime::instance();
#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    const bool benchmarkRunning =
        artemis::benchmark::BenchmarkRuntime::instance().running();
#else
    const bool benchmarkRunning = false;
#endif
    if (runtime.running()) {
        const bool advanced = runtime.extended();
        autoTuneAction->setText(advanced
            ? "artemis/performance/quick_auto_tune"_i18n
            : "artemis/performance/cancel_auto_tune"_i18n);
        autoTuneAdvanced->setText(advanced
            ? "artemis/performance/cancel_auto_tune"_i18n
            : "artemis/performance/advanced_auto_tune"_i18n);
        autoTuneAction->setFocusable(!advanced);
        autoTuneAdvanced->setFocusable(advanced);
        const char* stateKey = "artemis/performance/applying";
        switch (runtime.state()) {
        case artemis::benchmark::AutoTuneState::Reconnecting:
            stateKey = "artemis/performance/reconnecting";
            break;
        case artemis::benchmark::AutoTuneState::WarmingUp:
            stateKey = "artemis/performance/warming_up";
            break;
        case artemis::benchmark::AutoTuneState::Measuring:
            stateKey = "artemis/performance/measuring";
            break;
        case artemis::benchmark::AutoTuneState::ApplyingBest:
            stateKey = "artemis/performance/applying_best";
            break;
        default:
            break;
        }
        autoTuneSummary->setDetailText(fmt::format(
            "{} · {} · {} / {}",
            advanced ? "artemis/performance/advanced_mode"_i18n
                     : "artemis/performance/quick_mode"_i18n,
            getStr(stateKey),
            std::min(runtime.currentStep() + 1, runtime.totalSteps()),
            runtime.totalSteps()));
        return;
    }

    autoTuneAction->setText("artemis/performance/quick_auto_tune"_i18n);
    autoTuneAdvanced->setText("artemis/performance/advanced_auto_tune"_i18n);
    autoTuneAction->setFocusable(runtime.available() && !benchmarkRunning);
    autoTuneAdvanced->setFocusable(runtime.available() && !benchmarkRunning);
    if (runtime.state() == artemis::benchmark::AutoTuneState::Failed) {
        autoTuneSummary->setDetailText(
            "artemis/performance/auto_tune_failed"_i18n);
    } else if (runtime.state() == artemis::benchmark::AutoTuneState::Cancelled) {
        autoTuneSummary->setDetailText(
            "artemis/performance/auto_tune_cancelled"_i18n);
    } else if (runtime.state() ==
               artemis::benchmark::AutoTuneState::NoStableProfile) {
        autoTuneSummary->setDetailText(
            "artemis/performance/no_stable_profile"_i18n);
    } else if (const auto best = runtime.recommendation()) {
        autoTuneSummary->setDetailText(fmt::format(
            "{}x{} · {} FPS · {:.0f} Mbps",
            best->profile.width, best->profile.height, best->profile.fps,
            static_cast<double>(best->profile.bitrateKbps) / 1000.0));
    } else if (runtime.available()) {
        autoTuneSummary->setDetailText("artemis/performance/auto_tune_ready"_i18n);
    } else {
        autoTuneSummary->setDetailText("artemis/performance/auto_tune_unavailable"_i18n);
    }
#endif
}

void PerformanceTab::refresh() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active()) {
        network->setDetailText(
            bitrateRestartPending && session && !session->is_terminated()
                ? "artemis/performance/reconnecting_bitrate"_i18n
                : "-");
        network->setFocusable(false);
        receiveLatency->setDetailText("-");
        decodeLatency->setDetailText("-");
        renderLatency->setDetailText("-");
        packetLoss->setDetailText("-");
        renderedFps->setDetailText("-");
        queueDepth->setDetailText("-");
        benchmarkAction->setFocusable(false);
        benchmarkSave->setFocusable(false);
        benchmarkReset->setFocusable(false);
#if ARTEMIS_HAS_AUTO_TUNE
        if (artemis::benchmark::AutoTuneRuntime::instance().running()) {
            autoTuneSummary->setVisibility(Visibility::VISIBLE);
            updateAutoTuneStatus();
        } else {
            autoTuneSummary->setVisibility(Visibility::GONE);
            autoTuneAction->setFocusable(false);
            autoTuneAdvanced->setFocusable(false);
        }
#else
        autoTuneSummary->setVisibility(Visibility::GONE);
        autoTuneAction->setFocusable(false);
        autoTuneAdvanced->setFocusable(false);
#endif
        return;
    }

    bitrateRestartPending = false;
    network->setFocusable(true);

    autoTuneSummary->setVisibility(Visibility::VISIBLE);

#if ARTEMIS_HAS_BENCHMARK_RUNTIME
    benchmarkAction->setFocusable(true);
#endif
#if ARTEMIS_HAS_AUTO_TUNE
    autoTuneAction->setFocusable(artemis::benchmark::AutoTuneRuntime::instance().available() ||
                                 artemis::benchmark::AutoTuneRuntime::instance().running());
    autoTuneAdvanced->setFocusable(
        artemis::benchmark::AutoTuneRuntime::instance().available() ||
        artemis::benchmark::AutoTuneRuntime::instance().running());
#endif

    const auto* stats = session->session_stats();
    const auto& decode = stats->video_decode_stats;
    const auto& render = stats->video_render_stats;

    const uint64_t currentReceived = decode.total_received_frames;
    const uint64_t currentDropped = decode.network_dropped_frames;
    uint64_t receivedDelta = 0;
    uint64_t droppedDelta = 0;
    if (previousReceived != 0 || previousDropped != 0) {
        receivedDelta = currentReceived >= previousReceived
            ? currentReceived - previousReceived : 0;
        droppedDelta = currentDropped >= previousDropped
            ? currentDropped - previousDropped : 0;
    }
    previousReceived = currentReceived;
    previousDropped = currentDropped;

    const uint64_t networkTotal = receivedDelta + droppedDelta;
    const double lossPercent = networkTotal > 0
        ? static_cast<double>(droppedDelta) * 100.0 / static_cast<double>(networkTotal)
        : 0.0;

    artemis::performance::LitePerformanceSnapshot snapshot;
    snapshot.networkMbps = static_cast<double>(Settings::instance().bitrate()) / 1000.0;
    snapshot.receiveLatencyMs = decode.current_receive_time;
    snapshot.decodeLatencyMs = decode.current_decoding_time;
    snapshot.packetLossPercent = lossPercent;
    snapshot.renderedFps = render.rendered_fps;
    const auto lite = artemis::performance::buildLiteStatus(snapshot);

    network->setDetailText(lite.networkText);
    receiveLatency->setDetailText(lite.latencyText);
    decodeLatency->setDetailText(lite.decodeText);
    renderLatency->setDetailText(fmt::format("{:.2f} ms", render.rendering_time));
    packetLoss->setDetailText(lite.packetLossText);
    renderedFps->setDetailText(lite.fpsText);
    queueDepth->setDetailText(fmt::format("{} / {} / {}",
        AVFrameHolder::instance().getFrameQueueSize(),
        AVFrameHolder::instance().getFrameQueueTargetDepth(),
        AVFrameHolder::instance().getFrameQueueCapacity()));

    const auto color = lite.healthy
        ? Application::getTheme()["brls/list/listItem_value_color"]
        : Application::getTheme()["brls/accent"];
    packetLoss->setDetailTextColor(color);
    renderedFps->setDetailTextColor(color);

    updateBenchmarkStatus();
    updateAutoTuneStatus();
}

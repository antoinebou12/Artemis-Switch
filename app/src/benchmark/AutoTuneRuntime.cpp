#include "AutoTuneRuntime.hpp"

#if __has_include("benchmark/BenchmarkRuntime.hpp")
#define ARTEMIS_AUTOTUNE_HAS_BENCHMARK 1
#include "benchmark/BenchmarkRuntime.hpp"
#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <borealis.hpp>
#include <chrono>
#else
#define ARTEMIS_AUTOTUNE_HAS_BENCHMARK 0
#endif

#if __has_include("streaming/StreamProfileStore.hpp")
#include "streaming/StreamProfileStore.hpp"
#define ARTEMIS_AUTOTUNE_HAS_CUSTOM_PROFILE 1
#else
#define ARTEMIS_AUTOTUNE_HAS_CUSTOM_PROFILE 0
#endif

namespace artemis::benchmark {

AutoTuneRuntime& AutoTuneRuntime::instance() {
    static AutoTuneRuntime runtime;
    return runtime;
}

AutoTuneRuntime::~AutoTuneRuntime() {
    cancel();
}

bool AutoTuneRuntime::available() const {
#if ARTEMIS_AUTOTUNE_HAS_BENCHMARK
    auto* session = MoonlightSession::activeSession();
    return session && session->is_active();
#else
    return false;
#endif
}

bool AutoTuneRuntime::start(bool extended) {
#if ARTEMIS_AUTOTUNE_HAS_BENCHMARK
    if (!available())
        return false;

    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return false;

    if (m_worker.joinable())
        m_worker.join();

    m_cancelRequested.store(false);
    {
        std::lock_guard lock(m_mutex);
        m_recommendation.reset();
        m_session.reset(extended ? AutoTunePlan::extendedSwitchPlan()
                                 : AutoTunePlan::quickSwitchPlan());
    }

    m_worker = std::thread([this, extended] { worker(extended); });
    return true;
#else
    (void)extended;
    return false;
#endif
}

void AutoTuneRuntime::cancel() {
    m_cancelRequested.store(true);
    if (m_worker.joinable())
        m_worker.join();
    m_running.store(false);
}

size_t AutoTuneRuntime::currentStep() const {
    std::lock_guard lock(m_mutex);
    return m_session.currentIndex();
}

size_t AutoTuneRuntime::totalSteps() const {
    std::lock_guard lock(m_mutex);
    return m_session.totalSteps();
}

std::optional<AutoTuneResult> AutoTuneRuntime::recommendation() const {
    std::lock_guard lock(m_mutex);
    return m_recommendation;
}

void AutoTuneRuntime::worker(bool extended) {
#if ARTEMIS_AUTOTUNE_HAS_BENCHMARK
    (void)extended;

    const int originalResolution = Settings::instance().resolution();
    const int originalFps = Settings::instance().fps();
    const int originalBitrate = Settings::instance().bitrate();
    const int originalThreads = Settings::instance().decoder_threads();
    const VideoCodec originalCodec = Settings::instance().video_codec();

#if ARTEMIS_AUTOTUNE_HAS_CUSTOM_PROFILE
    const auto originalCustomProfile =
        artemis::streaming::StreamProfileStore::instance().get();
    if (originalCustomProfile.customResolutionEnabled) {
        artemis::streaming::StreamProfileStore::instance().setCustomResolution(
            false, originalCustomProfile.width, originalCustomProfile.height);
    }
#endif

    auto sleepCancelable = [this](int milliseconds) {
        int remaining = milliseconds;
        while (remaining > 0 && !m_cancelRequested.load()) {
            const int slice = std::min(remaining, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            remaining -= slice;
        }
        return !m_cancelRequested.load();
    };

    auto applyProfile = [](const AutoTuneProfile& profile) {
        brls::sync([profile] {
            Settings::instance().set_resolution(profile.height);
            Settings::instance().set_fps(profile.fps);
            Settings::instance().set_bitrate(profile.bitrateKbps);
            Settings::instance().set_decoder_threads(profile.decoderThreads);
            Settings::instance().set_video_codec(profile.codec == "H264" ? H264 : H265);
            Settings::instance().save();
            if (auto* session = MoonlightSession::activeSession())
                session->restart();
        });
    };

    while (!m_cancelRequested.load()) {
        AutoTuneStep step;
        {
            std::lock_guard lock(m_mutex);
            const auto* current = m_session.current();
            if (!current)
                break;
            step = *current;
        }

        applyProfile(step.profile);
        if (!sleepCancelable(2000))
            break;

        auto* stream = MoonlightSession::activeSession();
        int waitMs = 8000;
        while (waitMs > 0 && !m_cancelRequested.load() &&
               (!stream || !stream->is_active())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waitMs -= 100;
            stream = MoonlightSession::activeSession();
        }
        if (!stream || !stream->is_active() || m_cancelRequested.load())
            break;

        if (!sleepCancelable(step.warmupSeconds * 1000))
            break;

        auto& benchmark = BenchmarkRuntime::instance();
        benchmark.start(step.profile.fps);
        if (!sleepCancelable(step.benchmarkSeconds * 1000)) {
            benchmark.stop();
            break;
        }
        const auto summary = benchmark.stop();

        AutoTuneResult result;
        result.profile = step.profile;
        result.stabilityScore = summary.stabilityScore;
        result.networkDropPercent = summary.networkDropPercent;
        result.renderedFpsMean = summary.renderedFps.mean;
        result.clientP99Ms = summary.clientProcessingMs.p99;

        std::lock_guard lock(m_mutex);
        m_session.record(result);
    }

    std::optional<AutoTuneResult> best;
    {
        std::lock_guard lock(m_mutex);
        best = m_session.recommendation();
        m_recommendation = best;
    }

    if (!m_cancelRequested.load() && best) {
        applyProfile(best->profile);
    } else {
#if ARTEMIS_AUTOTUNE_HAS_CUSTOM_PROFILE
        artemis::streaming::StreamProfileStore::instance().setCustomResolution(
            originalCustomProfile.customResolutionEnabled,
            originalCustomProfile.width,
            originalCustomProfile.height);
#endif
        brls::sync([=] {
            Settings::instance().set_resolution(originalResolution);
            Settings::instance().set_fps(originalFps);
            Settings::instance().set_bitrate(originalBitrate);
            Settings::instance().set_decoder_threads(originalThreads);
            Settings::instance().set_video_codec(originalCodec);
            Settings::instance().save();
            if (auto* session = MoonlightSession::activeSession())
                session->restart();
        });
    }

    m_running.store(false);
#else
    (void)extended;
    m_running.store(false);
#endif
}

} // namespace artemis::benchmark

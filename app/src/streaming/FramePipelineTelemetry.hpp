#pragma once

#include "Singleton.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace artemis::streaming {

// Per-stream pipeline / startup telemetry shared by decoder, queue, renderer.
class FramePipelineTelemetry : public Singleton<FramePipelineTelemetry> {
public:
    using clock = std::chrono::steady_clock;

    void reset() {
        std::lock_guard lock(m_mutex);
        m_ = State{};
    }

    void noteStreamStart() {
        std::lock_guard lock(m_mutex);
        m_ = State{};
        m_.streamStart = clock::now();
        m_.streamStarted = true;
    }

    void noteFirstPacket(size_t bytes) {
        std::lock_guard lock(m_mutex);
        ensureStreamLocked();
        if (!m_.hasFirstPacket) {
            m_.firstPacket = clock::now();
            m_.hasFirstPacket = true;
        }
        accumulateBytesLocked(bytes);
    }

    void noteDecodedFrame(clock::time_point networkComplete,
                          clock::time_point decodeDone) {
        std::lock_guard lock(m_mutex);
        ensureStreamLocked();
        if (!m_.hasFirstDecode) {
            m_.firstDecode = decodeDone;
            m_.hasFirstDecode = true;
        }
        m_.pendingNetworkComplete = networkComplete;
        m_.pendingDecodeDone = decodeDone;
        m_.hasPendingTiming = true;

        const double decodeMs = msBetween(networkComplete, decodeDone);
        updateEma(m_.decodeMsEma, decodeMs);
    }

    // Called by AVFrameQueue on push; returns pending timing if any.
    bool consumePendingDecodeTiming(clock::time_point& networkComplete,
                                    clock::time_point& decodeDone) {
        std::lock_guard lock(m_mutex);
        if (!m_.hasPendingTiming)
            return false;
        networkComplete = m_.pendingNetworkComplete;
        decodeDone = m_.pendingDecodeDone;
        m_.hasPendingTiming = false;
        return true;
    }

    void noteFrameQueued(clock::time_point queuedAt) {
        std::lock_guard lock(m_mutex);
        (void)queuedAt;
    }

    void noteFrameSelected(clock::time_point networkComplete,
                           clock::time_point decodeDone,
                           clock::time_point selectedAt) {
        std::lock_guard lock(m_mutex);
        const double queueWaitMs = msBetween(decodeDone, selectedAt);
        updateEma(m_.queueWaitMsEma, queueWaitMs);
        updateP95(m_.queueWaitMsP95, queueWaitMs);
        m_.lastSelectedAt = selectedAt;
        m_.hasLastSelected = true;
        m_.lastDecodeDoneForSelected = decodeDone;
        m_.lastNetworkCompleteForSelected = networkComplete;
        m_.hasLastNetworkComplete = true;
    }

    void noteFramePresented(double renderMs, double gpuSubmitMs) {
        std::lock_guard lock(m_mutex);
        ensureStreamLocked();
        const auto presentedAt = clock::now();
        if (!m_.hasFirstPresent) {
            m_.firstPresent = presentedAt;
            m_.hasFirstPresent = true;
        }

        if (m_.hasLastNetworkComplete) {
            const double clientMs =
                msBetween(m_.lastNetworkCompleteForSelected, presentedAt);
            updateEma(m_.clientPipelineMsEma, clientMs);
            updateP95(m_.clientPipelineMsP95, clientMs);
        }

        if (std::isfinite(renderMs) && renderMs >= 0.0) {
            updateEma(m_.renderMsEma, renderMs);
            updateP95(m_.renderMsP95, renderMs);
            m_.havePresentCostSamples = true;
        }
        if (std::isfinite(gpuSubmitMs) && gpuSubmitMs >= 0.0) {
            updateEma(m_.gpuSubmitMsEma, gpuSubmitMs);
            updateP95(m_.gpuSubmitMsP95, gpuSubmitMs);
            m_.havePresentCostSamples = true;
        }

        if (m_.hasLastPresent) {
            const double intervalMs = msBetween(m_.lastPresentAt, presentedAt);
            if (intervalMs > 0.0 && intervalMs < 250.0) {
                updateEma(m_.presentIntervalMsEma, intervalMs);
            }
        }
        m_.lastPresentAt = presentedAt;
        m_.hasLastPresent = true;
    }

    void noteVideoBytes(size_t bytes) {
        std::lock_guard lock(m_mutex);
        accumulateBytesLocked(bytes);
    }

    struct Snapshot {
        float queueWaitMs = 0.0f;
        float queueWaitMsP95 = 0.0f;
        float clientPipelineMs = 0.0f;
        float clientPipelineMsP95 = 0.0f;
        float decodeMs = 0.0f;
        float renderMsP95 = 0.0f;
        float gpuSubmitMsP95 = 0.0f;
        float presentIntervalMs = 0.0f;
        float actualVideoMbps = 0.0f;
        bool havePresentCostSamples = false;
        bool hasFirstPresent = false;
        float timeToFirstPacketMs = -1.0f;
        float timeToFirstDecodeMs = -1.0f;
        float timeToFirstPresentMs = -1.0f;
    };

    Snapshot snapshot() const {
        std::lock_guard lock(m_mutex);
        Snapshot s;
        s.queueWaitMs = static_cast<float>(m_.queueWaitMsEma);
        s.queueWaitMsP95 = static_cast<float>(m_.queueWaitMsP95);
        s.clientPipelineMs = static_cast<float>(m_.clientPipelineMsEma);
        s.clientPipelineMsP95 = static_cast<float>(m_.clientPipelineMsP95);
        s.decodeMs = static_cast<float>(m_.decodeMsEma);
        s.renderMsP95 = static_cast<float>(m_.renderMsP95);
        s.gpuSubmitMsP95 = static_cast<float>(m_.gpuSubmitMsP95);
        s.presentIntervalMs = static_cast<float>(m_.presentIntervalMsEma);
        s.havePresentCostSamples = m_.havePresentCostSamples;
        s.hasFirstPresent = m_.hasFirstPresent;
        s.actualVideoMbps = currentMbpsLocked();
        if (m_.streamStarted && m_.hasFirstPacket)
            s.timeToFirstPacketMs =
                static_cast<float>(msBetween(m_.streamStart, m_.firstPacket));
        if (m_.streamStarted && m_.hasFirstDecode)
            s.timeToFirstDecodeMs =
                static_cast<float>(msBetween(m_.streamStart, m_.firstDecode));
        if (m_.streamStarted && m_.hasFirstPresent)
            s.timeToFirstPresentMs =
                static_cast<float>(msBetween(m_.streamStart, m_.firstPresent));
        return s;
    }

private:
    struct State {
        bool streamStarted = false;
        clock::time_point streamStart{};
        bool hasFirstPacket = false;
        clock::time_point firstPacket{};
        bool hasFirstDecode = false;
        clock::time_point firstDecode{};
        bool hasFirstPresent = false;
        clock::time_point firstPresent{};

        bool hasPendingTiming = false;
        clock::time_point pendingNetworkComplete{};
        clock::time_point pendingDecodeDone{};

        bool hasLastSelected = false;
        clock::time_point lastSelectedAt{};
        clock::time_point lastDecodeDoneForSelected{};
        clock::time_point lastNetworkCompleteForSelected{};
        bool hasLastNetworkComplete = false;

        bool hasLastPresent = false;
        clock::time_point lastPresentAt{};

        double decodeMsEma = 0.0;
        double queueWaitMsEma = 0.0;
        double queueWaitMsP95 = 0.0;
        double clientPipelineMsEma = 0.0;
        double clientPipelineMsP95 = 0.0;
        double renderMsEma = 0.0;
        double renderMsP95 = 0.0;
        double gpuSubmitMsEma = 0.0;
        double gpuSubmitMsP95 = 0.0;
        double presentIntervalMsEma = 0.0;
        bool havePresentCostSamples = false;

        uint64_t windowBytes = 0;
        clock::time_point windowStart{};
        bool windowStarted = false;
        float lastMbps = 0.0f;
    };

    mutable std::mutex m_mutex;
    State m_;

    void ensureStreamLocked() {
        if (!m_.streamStarted) {
            m_.streamStart = clock::now();
            m_.streamStarted = true;
        }
    }

    void accumulateBytesLocked(size_t bytes) {
        ensureStreamLocked();
        const auto now = clock::now();
        if (!m_.windowStarted) {
            m_.windowStart = now;
            m_.windowStarted = true;
            m_.windowBytes = 0;
        }
        m_.windowBytes += static_cast<uint64_t>(bytes);
        const double elapsedSec =
            std::chrono::duration<double>(now - m_.windowStart).count();
        if (elapsedSec >= 0.25) {
            m_.lastMbps = static_cast<float>((m_.windowBytes * 8.0) /
                                             (elapsedSec * 1'000'000.0));
            m_.windowBytes = 0;
            m_.windowStart = now;
        }
    }

    float currentMbpsLocked() const { return m_.lastMbps; }

    static double msBetween(clock::time_point a, clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }

    static void updateEma(double& ema, double sample, double alpha = 0.2) {
        if (!std::isfinite(sample) || sample < 0.0)
            return;
        if (ema <= 0.0)
            ema = sample;
        else
            ema = ema * (1.0 - alpha) + sample * alpha;
    }

    // Cheap approximate p95 via high-water EMA (good enough for gate lead).
    static void updateP95(double& p95, double sample, double alphaUp = 0.35,
                          double alphaDown = 0.05) {
        if (!std::isfinite(sample) || sample < 0.0)
            return;
        if (p95 <= 0.0) {
            p95 = sample;
            return;
        }
        if (sample >= p95)
            p95 = p95 * (1.0 - alphaUp) + sample * alphaUp;
        else
            p95 = p95 * (1.0 - alphaDown) + sample * alphaDown;
    }
};

} // namespace artemis::streaming

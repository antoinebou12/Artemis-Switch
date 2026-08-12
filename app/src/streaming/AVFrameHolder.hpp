#pragma once

#include "Singleton.hpp"
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <optional>
#include "Settings.hpp"
#include <mutex>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
}

// Shared frame-storage entry for both pacing paths.
// The legacy path writes a zero-initialised timeEstimate and never reads it.
// The low-latency path writes the predicted arrival time of the *next* frame
// so that pop() can release this frame at the right moment.
struct TimedFrame {
    std::chrono::steady_clock::time_point timeEstimate{};
    std::chrono::steady_clock::time_point networkComplete{};
    std::chrono::steady_clock::time_point decodeDone{};
    std::chrono::steady_clock::time_point queued{};
    AVFrame* frame = nullptr;
};

class AVFrameQueue {
public:
    explicit AVFrameQueue();
    ~AVFrameQueue();

    bool push(AVFrame* item);
    bool pushTransferred(AVFrame* item);
    AVFrame* pop(bool* consumed = nullptr);
    AVFrame* acquireWriteFrame();
    void recycleWriteFrame(AVFrame*& frame);

    // The new lowLatency parameter selects the pacing path at configure time.
    // Defaults to false so all existing call-sites stay compatible.
    void configure(size_t queueLimit, int streamFps,
                   bool transferOwnershipEnabled,
                   bool lowLatency = false);

    static size_t capacityFor(size_t configuredQueueSize);

    // --- Stats getters (all existing names preserved) ---
    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t targetDepth() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t getFakeFrameUsage() const;
    [[nodiscard]] size_t getFramesDropStat() const;
    [[nodiscard]] size_t getEmptyQueueStat() const;
    [[nodiscard]] size_t getRebufferHoldStat() const;
    [[nodiscard]] size_t getOverflowDropStat() const;
    [[nodiscard]] size_t getPacingSkipStat() const;
    [[nodiscard]] size_t getScheduledHoldStat() const;
    [[nodiscard]] size_t getMaxPushBurstStat() const;
    [[nodiscard]] size_t getLocalClockPacedFrameStat() const;
    [[nodiscard]] size_t getPlayoutResyncStat() const;
    [[nodiscard]] double getEstimatedSourceFps() const;
    // Low-latency jitter estimate in milliseconds (0 when legacy path is active).
    [[nodiscard]] double getJitterMs() const;

    // Feed measured present cost into the adaptive low-latency gate.
    void updatePresentCostMs(double renderP95Ms, double gpuSubmitP95Ms,
                             bool haveSamples);

    void cleanup();

private:
    friend class AVFrameHolder;

    // Frame pool helpers
    AVFrame* acquireFrameLocked();

    // Shared push helper (no copy, caller transfers ownership of item)
    bool pushTransferredLocked(AVFrame* item);

    // Legacy arrival-rate estimator
    void recordArrivalLocked(std::chrono::steady_clock::time_point now);
    void resetArrivalRateEstimatorLocked();
    void trimToPlayoutWindowLocked();

    // Low-latency alpha-beta filter
    void updateLLFilterLocked(std::chrono::steady_clock::time_point now,
                               std::chrono::steady_clock::time_point& outNextEstimate);
    void resetLLFilterLocked();

    // ---- Queue storage (both paths share this deque) ----
    std::deque<TimedFrame> queue;
    std::queue<AVFrame*>   freeQueue;
    AVFrame*               bufferFrame = nullptr;

    // ---- Configuration ----
    size_t limit               = 0;
    size_t targetBufferedFrames = 0;
    int    streamFps           = 0;
    bool   transferOwnership   = false;
    bool   lowLatency          = false;

    // ---- Legacy pacing state ----
    std::chrono::nanoseconds              adaptiveFrameInterval{0};
    std::chrono::steady_clock::time_point lastDraw{};
    std::chrono::nanoseconds              averageDrawInterval{0};
    std::chrono::steady_clock::time_point arrivalWindowStart{};
    std::chrono::steady_clock::time_point lastArrival{};
    size_t arrivalWindowFrames  = 0;
    size_t arrivalRateSamples   = 0;
    double estimatedSourceFps   = 0.0;
    double frameCredit          = 0.0;
    bool   drawClockStarted     = false;
    bool   arrivalClockStarted  = false;
    bool   startupBuffering     = true;
    bool   playoutResyncNeeded  = true;

    // ---- Low-latency alpha-beta filter state ----
    bool   ll_firstArrival       = true;
    double ll_smoothedIntervalNs = 0.0;   // filtered inter-frame interval (ns)
    double ll_intervalVelocityNs = 0.0;   // rate-of-change term
    double ll_jitterNs           = 0.0;   // EMA of absolute prediction error (ns)
    std::chrono::steady_clock::time_point ll_lastArrival{};

    // Adaptive present-cost samples (from renderer telemetry).
    double ll_renderP95Ms = 0.0;
    double ll_gpuSubmitP95Ms = 0.0;
    bool   ll_havePresentCost = false;

    mutable std::mutex m_mutex;

    // ---- Statistics ----
    size_t fakeFrameUsedStat       = 0;
    size_t framesDroppedStat       = 0;
    size_t emptyQueueStat          = 0;
    size_t rebufferHoldStat        = 0;
    size_t overflowDropStat        = 0;
    size_t pacingSkipStat          = 0;
    size_t scheduledHoldStat       = 0;
    size_t pushesSincePop          = 0;
    size_t maxPushBurstStat        = 0;
    size_t localClockPacedFrameStat = 0;
    size_t playoutResyncStat       = 0;
};

class AVFrameHolder : public Singleton<AVFrameHolder> {
  public:
    void push(AVFrame* frame) {
        m_frame_queue.push(frame);
    }

    void pushTransferred(AVFrame* frame) {
        m_frame_queue.pushTransferred(frame);
    }

    void get(const std::function<void(AVFrame*)>& fn) {
        auto frame = m_frame_queue.pop();
        if (frame) {
            fn(frame);
        }
    }

    AVFrame* acquireWriteFrame() {
        return m_frame_queue.acquireWriteFrame();
    }

    void recycleWriteFrame(AVFrame*& frame) {
        m_frame_queue.recycleWriteFrame(frame);
    }

    void prepare(int streamFps, bool transferOwnership = false) {
        m_frame_queue.configure(Settings::instance().frames_queue_size(),
                                streamFps, transferOwnership,
                                Settings::instance().low_latency_pacing());
    }

    void cleanup() {
        m_frame_queue.cleanup();
    }

    [[nodiscard]] size_t getFakeFrameStat() const { return m_frame_queue.getFakeFrameUsage(); }
    [[nodiscard]] size_t getFrameDropStat() const { return m_frame_queue.getFramesDropStat(); }
    [[nodiscard]] size_t getFrameQueueSize() const { return m_frame_queue.size(); }
    [[nodiscard]] size_t getFrameQueueTargetDepth() const { return m_frame_queue.targetDepth(); }
    [[nodiscard]] size_t getFrameQueueCapacity() const { return m_frame_queue.capacity(); }
    [[nodiscard]] size_t getFrameQueueEmptyStat() const { return m_frame_queue.getEmptyQueueStat(); }
    [[nodiscard]] size_t getFrameQueueRebufferHoldStat() const { return m_frame_queue.getRebufferHoldStat(); }
    [[nodiscard]] size_t getFrameQueueOverflowDropStat() const { return m_frame_queue.getOverflowDropStat(); }
    [[nodiscard]] size_t getFrameQueuePacingSkipStat() const { return m_frame_queue.getPacingSkipStat(); }
    [[nodiscard]] size_t getFrameQueueScheduledHoldStat() const { return m_frame_queue.getScheduledHoldStat(); }
    [[nodiscard]] size_t getFrameQueueMaxPushBurstStat() const { return m_frame_queue.getMaxPushBurstStat(); }
    [[nodiscard]] size_t getFrameQueueLocalClockPacedFrameStat() const { return m_frame_queue.getLocalClockPacedFrameStat(); }
    [[nodiscard]] size_t getFrameQueuePlayoutResyncStat() const { return m_frame_queue.getPlayoutResyncStat(); }
    [[nodiscard]] double getFrameQueueEstimatedSourceFps() const { return m_frame_queue.getEstimatedSourceFps(); }
    [[nodiscard]] double getFrameQueueJitterMs() const { return m_frame_queue.getJitterMs(); }

    void updatePresentCostMs(double renderP95Ms, double gpuSubmitP95Ms,
                             bool haveSamples) {
        m_frame_queue.updatePresentCostMs(renderP95Ms, gpuSubmitP95Ms,
                                          haveSamples);
    }

  private:
    AVFrameQueue m_frame_queue;
};

//
//  AVFrameHolder.cpp
//  Artemis-Switch
//
// Dual-path frame queue:
//   Legacy path  – occupancy/frameCredit pacing (original Artemis behavior).
//   Low-latency path – alpha-beta filtered arrival estimation with a present-gate
//                      (ported from nyanpasu64/Moonlight-Switch frame-rate-sync).
//

#include "AVFrameHolder.hpp"
#include "FramePipelineTelemetry.hpp"
#include "PresentDeadline.hpp"

#include <algorithm>
#include <cmath>

namespace {

// ---- Shared ----------------------------------------------------------------

constexpr size_t kBurstHeadroomFrames = 5;

// ---- Legacy pacing constants -----------------------------------------------

constexpr auto   kArrivalRateWindow        = std::chrono::milliseconds(250);
constexpr auto   kArrivalRateResetGap      = std::chrono::milliseconds(500);
constexpr double kArrivalRateSmoothing     = 0.35;
constexpr double kOccupancyCorrectionPerFrame  = 0.01;
constexpr double kMaximumOccupancyCorrection   = 0.08;

// ---- Low-latency alpha-beta constants --------------------------------------

// Slow-path smoothing factor (used when measured interval is close to predicted).
constexpr double kLL_Alpha = 1.0 / 8.0;
// Fast convergence factor (used when the deviation is large, e.g. after a gap).
constexpr double kLL_FastAlpha = 1.0 / 4.0;

// ---- Helpers ---------------------------------------------------------------

void freeTimedFrameDeque(std::deque<TimedFrame>& frames) {
    for (auto& tf : frames) {
        if (tf.frame) {
            av_frame_free(&tf.frame);
        }
    }
    frames.clear();
}

void freeFrameQueue(std::queue<AVFrame*>& frames) {
    for (; !frames.empty(); frames.pop()) {
        AVFrame* frame = frames.front();
        av_frame_free(&frame);
    }
}

void recycleFrame(std::queue<AVFrame*>& freeQueue, AVFrame*& frame) {
    if (!frame) {
        return;
    }
    av_frame_unref(frame);
    freeQueue.push(frame);
    frame = nullptr;
}

TimedFrame makeTimedFrame(AVFrame* frame,
                          std::chrono::steady_clock::time_point timeEstimate,
                          std::chrono::steady_clock::time_point now) {
    TimedFrame tf;
    tf.timeEstimate = timeEstimate;
    tf.queued = now;
    tf.frame = frame;
    std::chrono::steady_clock::time_point networkComplete{};
    std::chrono::steady_clock::time_point decodeDone{};
    if (artemis::streaming::FramePipelineTelemetry::instance()
            .consumePendingDecodeTiming(networkComplete, decodeDone)) {
        tf.networkComplete = networkComplete;
        tf.decodeDone = decodeDone;
    } else {
        tf.networkComplete = now;
        tf.decodeDone = now;
    }
    artemis::streaming::FramePipelineTelemetry::instance().noteFrameQueued(now);
    return tf;
}

} // namespace

// ===========================================================================
// AVFrameQueue
// ===========================================================================

AVFrameQueue::AVFrameQueue() {}

AVFrameQueue::~AVFrameQueue() {
    cleanup();
    freeFrameQueue(freeQueue);
}

size_t AVFrameQueue::capacityFor(size_t configuredQueueSize) {
    return std::max<size_t>(configuredQueueSize, 1) + kBurstHeadroomFrames;
}

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------

void AVFrameQueue::configure(size_t queueLimit, int configuredStreamFps,
                             bool transferOwnershipEnabled,
                             bool lowLatencyEnabled) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const size_t configuredDepth = std::max<size_t>(queueLimit, 1);
    limit         = capacityFor(configuredDepth);
    lowLatency    = lowLatencyEnabled;

    if (lowLatency) {
        // Cap buffering at 1 to minimise presentation latency.
        targetBufferedFrames = configuredDepth > 1
            ? std::min<size_t>(configuredDepth - 1, 1) : 0;
    } else {
        targetBufferedFrames = configuredDepth > 1
            ? std::min<size_t>(configuredDepth - 1, 2) : 0;
    }

    transferOwnership        = transferOwnershipEnabled;
    streamFps                = configuredStreamFps;
    adaptiveFrameInterval    = std::chrono::nanoseconds::zero();
    drawClockStarted         = false;
    averageDrawInterval      = std::chrono::nanoseconds::zero();
    arrivalClockStarted      = false;
    arrivalWindowFrames      = 0;
    arrivalRateSamples       = 0;
    estimatedSourceFps       = 0.0;
    frameCredit              = 0.0;
    startupBuffering         = true;
    playoutResyncNeeded      = true;

    resetLLFilterLocked();
}

// ---------------------------------------------------------------------------
// push  (copy-ref variant)
// ---------------------------------------------------------------------------

bool AVFrameQueue::push(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!item) {
        return false;
    }

    if (transferOwnership) {
        return pushTransferredLocked(item);
    }

    AVFrame* queuedFrame = acquireFrameLocked();
    if (!queuedFrame) {
        return false;
    }

    if (av_frame_ref(queuedFrame, item) < 0) {
        av_frame_free(&queuedFrame);
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point timeEstimate{};
    if (lowLatency) {
        updateLLFilterLocked(now, timeEstimate);
    } else {
        recordArrivalLocked(now);
    }

    queue.push_back(makeTimedFrame(queuedFrame, timeEstimate, now));
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        const size_t keepFrames = targetBufferedFrames + 1;
        while (queue.size() > keepFrames) {
            AVFrame* dropped = queue.front().frame;
            queue.pop_front();
            recycleFrame(freeQueue, dropped);
            framesDroppedStat++;
            overflowDropStat++;
        }
        if (lowLatency) {
            resetLLFilterLocked();
        } else {
            resetArrivalRateEstimatorLocked();
            playoutResyncNeeded = true;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// pushTransferred  (public lock wrapper)
// ---------------------------------------------------------------------------

bool AVFrameQueue::pushTransferred(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pushTransferredLocked(item);
}

// ---------------------------------------------------------------------------
// pop
// ---------------------------------------------------------------------------

AVFrame* AVFrameQueue::pop(bool* consumed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    pushesSincePop = 0;

    if (consumed) {
        *consumed = false;
    }

    const auto now = std::chrono::steady_clock::now();

    // ========================================================================
    // LOW-LATENCY PATH
    // ========================================================================
    if (lowLatency) {
        // Aggressive latest-frame-wins: if the renderer is behind, skip
        // straight to the newest complete frame.
        while (queue.size() > 1) {
            AVFrame* dropped = queue.front().frame;
            queue.pop_front();
            recycleFrame(freeQueue, dropped);
            framesDroppedStat++;
            pacingSkipStat++;
        }

        if (queue.empty()) {
            if (bufferFrame) {
                fakeFrameUsedStat++;
                emptyQueueStat++;
            }
            return bufferFrame;
        }

        const double jitterMs = ll_jitterNs / 1.0e6;
        const double leadMs = artemis::streaming::presentGateLeadMs(
            ll_renderP95Ms, ll_gpuSubmitP95Ms, ll_havePresentCost, jitterMs);
        const auto gateNs = std::chrono::nanoseconds(
            static_cast<int64_t>(std::max(0.0, leadMs) * 1.0e6));

        if (now < queue.front().timeEstimate - gateNs) {
            scheduledHoldStat++;
            return bufferFrame;
        }

        TimedFrame selected = queue.front();
        queue.pop_front();
        artemis::streaming::FramePipelineTelemetry::instance().noteFrameSelected(
            selected.networkComplete, selected.decodeDone, now);
        recycleFrame(freeQueue, bufferFrame);
        bufferFrame = selected.frame;
        // Preserve timing on the buffer frame via telemetry last-select path;
        // renderer reads networkComplete from the last selected stamp.
        localClockPacedFrameStat++;
        if (consumed) {
            *consumed = true;
        }
        return bufferFrame;
    }

    // ========================================================================
    // LEGACY OCCUPANCY / FRAME-CREDIT PATH
    // ========================================================================

    if (!drawClockStarted) {
        lastDraw       = now;
        drawClockStarted = true;
    } else {
        const auto drawInterval =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDraw);
        lastDraw = now;

        // Ignore pathological intervals (app suspension, debugger pauses).
        if (drawInterval <= std::chrono::nanoseconds::zero() ||
            drawInterval > std::chrono::milliseconds(250)) {
            averageDrawInterval = std::chrono::nanoseconds::zero();
            frameCredit         = 0.0;
            playoutResyncNeeded = true;
        } else if (averageDrawInterval == std::chrono::nanoseconds::zero()) {
            averageDrawInterval = drawInterval;
        } else {
            averageDrawInterval =
                (averageDrawInterval * 15 + drawInterval) / 16;
        }
    }

    if (startupBuffering && queue.size() <= targetBufferedFrames) {
        if (bufferFrame) {
            fakeFrameUsedStat++;
            rebufferHoldStat++;
        }
        return bufferFrame;
    }

    if (startupBuffering) {
        startupBuffering    = false;
        frameCredit         = 0.0;
        playoutResyncNeeded = true;
    }

    size_t dueFrames = 0;
    const bool backlogResync = limit > 0 && queue.size() >= limit;

    if ((playoutResyncNeeded || backlogResync) && !queue.empty()) {
        trimToPlayoutWindowLocked();
        if (backlogResync) {
            resetArrivalRateEstimatorLocked();
        }
        playoutResyncNeeded = false;
        frameCredit         = 0.0;
        playoutResyncStat++;
        dueFrames           = 1;
    } else if (arrivalRateSamples == 0 ||
               adaptiveFrameInterval <= std::chrono::nanoseconds::zero() ||
               averageDrawInterval   <= std::chrono::nanoseconds::zero()) {
        dueFrames = queue.size() > targetBufferedFrames ? 1 : 0;
    } else {
        const double baseFramesPerDraw =
            static_cast<double>(averageDrawInterval.count()) /
            static_cast<double>(adaptiveFrameInterval.count());
        const double desiredDepth  = static_cast<double>(targetBufferedFrames + 1);
        const double depthError    = static_cast<double>(queue.size()) - desiredDepth;
        const double occupancyCorr = std::clamp(
            depthError * kOccupancyCorrectionPerFrame,
            -kMaximumOccupancyCorrection, kMaximumOccupancyCorrection);
        const double framesPerDraw = std::max(0.0, baseFramesPerDraw + occupancyCorr);

        if (baseFramesPerDraw >= 0.98 && baseFramesPerDraw <= 1.02 &&
            depthError == 0.0) {
            frameCredit = 0.0;
            dueFrames   = 1;
        } else {
            frameCredit += framesPerDraw;
            const size_t wholeFrames = static_cast<size_t>(frameCredit);
            frameCredit -= static_cast<double>(wholeFrames);
            dueFrames    = std::min(wholeFrames, limit);
        }
    }

    if (dueFrames == 0) {
        scheduledHoldStat++;
        return bufferFrame;
    }

    if (queue.empty()) {
        if (bufferFrame) {
            fakeFrameUsedStat++;
            emptyQueueStat++;
        }
        frameCredit         = 0.0;
        playoutResyncNeeded = true;
        resetArrivalRateEstimatorLocked();
        return bufferFrame;
    }

    const size_t consumeCount = std::min(queue.size(), dueFrames);

    recycleFrame(freeQueue, bufferFrame);
    for (size_t i = 0; i < consumeCount; i++) {
        AVFrame* item = queue.front().frame;
        queue.pop_front();

        if (i + 1 < consumeCount) {
            recycleFrame(freeQueue, item);
            framesDroppedStat++;
            pacingSkipStat++;
        } else {
            bufferFrame = item;
        }
    }

    if (consumed) {
        *consumed = true;
    }
    localClockPacedFrameStat++;
    return bufferFrame;
}

// ---------------------------------------------------------------------------
// acquireWriteFrame / recycleWriteFrame
// ---------------------------------------------------------------------------

AVFrame* AVFrameQueue::acquireWriteFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return acquireFrameLocked();
}

void AVFrameQueue::recycleWriteFrame(AVFrame*& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    recycleFrame(freeQueue, frame);
}

// ---------------------------------------------------------------------------
// Stats getters
// ---------------------------------------------------------------------------

size_t AVFrameQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return queue.size();
}

size_t AVFrameQueue::targetDepth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return targetBufferedFrames;
}

size_t AVFrameQueue::capacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return limit;
}

size_t AVFrameQueue::getFakeFrameUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return fakeFrameUsedStat;
}

size_t AVFrameQueue::getFramesDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return framesDroppedStat;
}

size_t AVFrameQueue::getEmptyQueueStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return emptyQueueStat;
}

size_t AVFrameQueue::getRebufferHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return rebufferHoldStat;
}

size_t AVFrameQueue::getOverflowDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return overflowDropStat;
}

size_t AVFrameQueue::getPacingSkipStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pacingSkipStat;
}

size_t AVFrameQueue::getScheduledHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return scheduledHoldStat;
}

size_t AVFrameQueue::getMaxPushBurstStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return maxPushBurstStat;
}

size_t AVFrameQueue::getLocalClockPacedFrameStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return localClockPacedFrameStat;
}

size_t AVFrameQueue::getPlayoutResyncStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return playoutResyncStat;
}

double AVFrameQueue::getEstimatedSourceFps() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (lowLatency && ll_smoothedIntervalNs > 0.0) {
        return 1.0e9 / ll_smoothedIntervalNs;
    }
    return estimatedSourceFps;
}

double AVFrameQueue::getJitterMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ll_jitterNs / 1.0e6;
}

void AVFrameQueue::updatePresentCostMs(double renderP95Ms, double gpuSubmitP95Ms,
                                       bool haveSamples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ll_renderP95Ms = renderP95Ms;
    ll_gpuSubmitP95Ms = gpuSubmitP95Ms;
    ll_havePresentCost = haveSamples;
}

// ---------------------------------------------------------------------------
// cleanup
// ---------------------------------------------------------------------------

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);

    fakeFrameUsedStat        = 0;
    framesDroppedStat        = 0;
    emptyQueueStat           = 0;
    rebufferHoldStat         = 0;
    overflowDropStat         = 0;
    pacingSkipStat           = 0;
    scheduledHoldStat        = 0;
    pushesSincePop           = 0;
    maxPushBurstStat         = 0;
    localClockPacedFrameStat = 0;
    playoutResyncStat        = 0;

    drawClockStarted         = false;
    averageDrawInterval      = std::chrono::nanoseconds::zero();
    arrivalClockStarted      = false;
    arrivalWindowFrames      = 0;
    arrivalRateSamples       = 0;
    estimatedSourceFps       = 0.0;
    adaptiveFrameInterval    = std::chrono::nanoseconds::zero();
    frameCredit              = 0.0;
    startupBuffering         = true;
    playoutResyncNeeded      = true;

    resetLLFilterLocked();
    ll_renderP95Ms = 0.0;
    ll_gpuSubmitP95Ms = 0.0;
    ll_havePresentCost = false;

    if (bufferFrame) {
        av_frame_free(&bufferFrame);
    }

    freeTimedFrameDeque(queue);
    freeFrameQueue(freeQueue);
    freeQueue = {};
}

// ---------------------------------------------------------------------------
// Private helpers – frame pool
// ---------------------------------------------------------------------------

AVFrame* AVFrameQueue::acquireFrameLocked() {
    if (!freeQueue.empty()) {
        AVFrame* frame = freeQueue.front();
        freeQueue.pop();
        av_frame_unref(frame);
        return frame;
    }
    return av_frame_alloc();
}

// ---------------------------------------------------------------------------
// pushTransferredLocked  (caller already holds m_mutex, item ownership transferred)
// ---------------------------------------------------------------------------

bool AVFrameQueue::pushTransferredLocked(AVFrame* item) {
    if (!item) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point timeEstimate{};
    if (lowLatency) {
        updateLLFilterLocked(now, timeEstimate);
    } else {
        recordArrivalLocked(now);
    }

    queue.push_back(makeTimedFrame(item, timeEstimate, now));
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        const size_t keepFrames = targetBufferedFrames + 1;
        while (queue.size() > keepFrames) {
            AVFrame* dropped = queue.front().frame;
            queue.pop_front();
            recycleFrame(freeQueue, dropped);
            framesDroppedStat++;
            overflowDropStat++;
        }
        if (lowLatency) {
            resetLLFilterLocked();
        } else {
            resetArrivalRateEstimatorLocked();
            playoutResyncNeeded = true;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Private helpers – legacy arrival-rate estimator
// ---------------------------------------------------------------------------

void AVFrameQueue::recordArrivalLocked(std::chrono::steady_clock::time_point now) {
    if (!arrivalClockStarted) {
        arrivalClockStarted  = true;
        arrivalWindowStart   = now;
        lastArrival          = now;
        arrivalWindowFrames  = 1;
        return;
    }

    if (now - lastArrival > kArrivalRateResetGap) {
        arrivalWindowStart  = now;
        lastArrival         = now;
        arrivalWindowFrames = 1;
        return;
    }

    lastArrival = now;
    arrivalWindowFrames++;
    const auto elapsed = now - arrivalWindowStart;
    if (elapsed < kArrivalRateWindow) {
        return;
    }

    if (arrivalWindowFrames > 1) {
        const double elapsedSeconds =
            std::chrono::duration<double>(elapsed).count();
        double sampleFps =
            static_cast<double>(arrivalWindowFrames - 1) / elapsedSeconds;
        const double maximumFps =
            streamFps > 0 ? static_cast<double>(streamFps) : 240.0;
        sampleFps = std::clamp(sampleFps, 1.0, maximumFps);

        if (arrivalRateSamples == 0) {
            estimatedSourceFps = sampleFps;
        } else {
            estimatedSourceFps =
                estimatedSourceFps * (1.0 - kArrivalRateSmoothing) +
                sampleFps * kArrivalRateSmoothing;
        }
        arrivalRateSamples++;
        adaptiveFrameInterval = std::chrono::nanoseconds(
            static_cast<int64_t>(1000000000.0 / estimatedSourceFps));
    }

    arrivalWindowStart  = now;
    arrivalWindowFrames = 1;
}

void AVFrameQueue::resetArrivalRateEstimatorLocked() {
    arrivalClockStarted = false;
    arrivalWindowFrames = 0;
    arrivalRateSamples  = 0;
    estimatedSourceFps  = 0.0;
    adaptiveFrameInterval = std::chrono::nanoseconds::zero();
}

void AVFrameQueue::trimToPlayoutWindowLocked() {
    const size_t keepFrames = targetBufferedFrames + 1;
    while (queue.size() > keepFrames) {
        AVFrame* dropped = queue.front().frame;
        queue.pop_front();
        recycleFrame(freeQueue, dropped);
        framesDroppedStat++;
        pacingSkipStat++;
    }
}

// ---------------------------------------------------------------------------
// Private helpers – low-latency alpha-beta filter
// ---------------------------------------------------------------------------

void AVFrameQueue::updateLLFilterLocked(
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point& outNextEstimate) {

    if (ll_firstArrival) {
        ll_firstArrival      = false;
        ll_lastArrival       = now;
        ll_smoothedIntervalNs = streamFps > 0
            ? 1.0e9 / static_cast<double>(streamFps)
            : 1.0e9 / 60.0;
        ll_intervalVelocityNs = 0.0;
        // Seed jitter at 10 % of nominal interval.
        ll_jitterNs = ll_smoothedIntervalNs * 0.1;
        outNextEstimate = now + std::chrono::nanoseconds(
            static_cast<int64_t>(ll_smoothedIntervalNs));
        return;
    }

    const double measuredNs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - ll_lastArrival).count());
    ll_lastArrival = now;

    // Predict where we expected the interval to be.
    const double predictedInterval = ll_smoothedIntervalNs + ll_intervalVelocityNs;
    const double predError         = measuredNs - predictedInterval;

    // Choose alpha: converge quickly when the measured interval jumps far from
    // the prediction (e.g. after a network pause), settle slowly otherwise.
    const double alpha = (std::abs(predError) > predictedInterval * 0.5)
        ? kLL_FastAlpha : kLL_Alpha;
    // Derive beta from alpha (standard alpha-beta relationship).
    const double beta = (alpha * alpha) / (2.0 - alpha);

    ll_smoothedIntervalNs  = predictedInterval + alpha * predError;
    ll_intervalVelocityNs += beta * predError;

    // Clamp interval to a sane range: no faster than 120 % of configured FPS,
    // no slower than one second.
    if (streamFps > 0) {
        const double minInterval =
            1.0e9 / (static_cast<double>(streamFps) * 1.2);
        ll_smoothedIntervalNs = std::max(ll_smoothedIntervalNs, minInterval);
    }
    ll_smoothedIntervalNs = std::min(ll_smoothedIntervalNs, 1.0e9);

    // Clamp velocity drift to ±20 % of the smoothed interval.
    const double velLimit = ll_smoothedIntervalNs * 0.2;
    ll_intervalVelocityNs =
        std::clamp(ll_intervalVelocityNs, -velLimit, velLimit);

    // Update jitter: EMA of the absolute prediction error.
    ll_jitterNs = ll_jitterNs * (1.0 - kLL_Alpha) +
                  std::abs(predError) * kLL_Alpha;

    outNextEstimate = now + std::chrono::nanoseconds(
        static_cast<int64_t>(ll_smoothedIntervalNs));
}

void AVFrameQueue::resetLLFilterLocked() {
    ll_firstArrival       = true;
    ll_smoothedIntervalNs = 0.0;
    ll_intervalVelocityNs = 0.0;
    ll_jitterNs           = 0.0;
    ll_lastArrival        = {};
}

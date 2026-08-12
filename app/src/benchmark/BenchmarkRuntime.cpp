#include "BenchmarkRuntime.hpp"

#include "AVFrameHolder.hpp"
#include "FramePipelineTelemetry.hpp"
#include "MoonlightSession.hpp"

#include <chrono>

namespace artemis::benchmark {

BenchmarkRuntime& BenchmarkRuntime::instance() {
    static BenchmarkRuntime runtime;
    return runtime;
}

BenchmarkRuntime::~BenchmarkRuntime() {
    if (m_running.load())
        stop();
}

bool BenchmarkRuntime::start(int targetFps) {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return false;

    {
        std::lock_guard lock(m_mutex);
        m_accumulator = BenchmarkAccumulator(targetFps);
    }

    m_worker = std::thread([this] { workerLoop(); });
    return true;
}

BenchmarkSummary BenchmarkRuntime::stop() {
    m_running.store(false);
    if (m_worker.joinable())
        m_worker.join();

    std::lock_guard lock(m_mutex);
    return m_accumulator.summarize();
}

void BenchmarkRuntime::reset() {
    if (m_running.load())
        stop();
    std::lock_guard lock(m_mutex);
    m_accumulator.reset();
}

size_t BenchmarkRuntime::sampleCount() const {
    std::lock_guard lock(m_mutex);
    return m_accumulator.size();
}

BenchmarkSummary BenchmarkRuntime::snapshot() const {
    std::lock_guard lock(m_mutex);
    return m_accumulator.summarize();
}

void BenchmarkRuntime::workerLoop() {
    while (m_running.load()) {
        captureSample();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void BenchmarkRuntime::captureSample() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active())
        return;

    const auto* stats = session->session_stats();
    if (!stats)
        return;

    const auto& decode = stats->video_decode_stats;
    const auto& render = stats->video_render_stats;
    const auto pipe =
        artemis::streaming::FramePipelineTelemetry::instance().snapshot();
    auto& holder = AVFrameHolder::instance();

    // Defer sampling until first frame presented (avoid warmup as degraded).
    if (!pipe.hasFirstPresent)
        return;

    BenchmarkSample sample;
    sample.timestampMs = LiGetMillis();
    sample.hostFps = decode.current_host_fps;
    sample.receivedFps = decode.current_received_fps;
    sample.decodedFps = decode.current_decoded_fps;
    sample.renderedFps = render.rendered_fps;
    sample.receiveMs = decode.current_receive_time;
    sample.decodeMs = decode.current_decoding_time;
    sample.decoderDelayMs = decode.current_decoder_delay;
    sample.renderMs = render.rendering_time;
    sample.gpuRenderMs = render.gpu_rendering_time;
    sample.queueDepth = static_cast<float>(holder.getFrameQueueSize());
    sample.queueTarget = static_cast<float>(holder.getFrameQueueTargetDepth());
    sample.queueJitterMs = static_cast<float>(holder.getFrameQueueJitterMs());
    sample.estimatedSourceFps =
        static_cast<float>(holder.getFrameQueueEstimatedSourceFps());
    sample.presentIntervalMs = pipe.presentIntervalMs;
    sample.queueWaitMs = decode.current_queue_wait_ms > 0.0f
                             ? decode.current_queue_wait_ms
                             : pipe.queueWaitMs;
    sample.clientPipelineMs = decode.current_client_pipeline_ms > 0.0f
                                  ? decode.current_client_pipeline_ms
                                  : pipe.clientPipelineMs;
    sample.actualVideoMbps = decode.current_video_mbps > 0.0f
                                 ? decode.current_video_mbps
                                 : pipe.actualVideoMbps;
    sample.timeToFirstPacketMs = pipe.timeToFirstPacketMs;
    sample.timeToFirstDecodeMs = pipe.timeToFirstDecodeMs;
    sample.timeToFirstPresentMs = pipe.timeToFirstPresentMs;
    sample.totalReceivedFrames = decode.total_received_frames;
    sample.networkDroppedFrames = decode.network_dropped_frames;
    sample.queueUnderflows = holder.getFakeFrameStat();
    sample.queueSkippedFrames = holder.getFrameDropStat();
    sample.queueOverflowDrops = holder.getFrameQueueOverflowDropStat();
    sample.queuePacingSkips = holder.getFrameQueuePacingSkipStat();
    sample.queueResyncs = holder.getFrameQueuePlayoutResyncStat();

    std::lock_guard lock(m_mutex);
    m_accumulator.addSample(sample);
}

} // namespace artemis::benchmark

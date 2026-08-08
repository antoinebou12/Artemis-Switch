#include "BenchmarkRuntime.hpp"

#include "AVFrameHolder.hpp"
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
    sample.totalReceivedFrames = decode.total_received_frames;
    sample.networkDroppedFrames = decode.network_dropped_frames;
    sample.queueUnderflows = AVFrameHolder::instance().getFakeFrameStat();
    sample.queueSkippedFrames = AVFrameHolder::instance().getFrameDropStat();
    sample.queueOverflowDrops = AVFrameHolder::instance().getFrameQueueOverflowDropStat();
    sample.queuePacingSkips = AVFrameHolder::instance().getFrameQueuePacingSkipStat();
    sample.queueResyncs = AVFrameHolder::instance().getFrameQueuePlayoutResyncStat();

    std::lock_guard lock(m_mutex);
    m_accumulator.addSample(sample);
}

} // namespace artemis::benchmark

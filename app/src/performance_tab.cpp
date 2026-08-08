#include "performance_tab.hpp"

#include "AVFrameHolder.hpp"
#include "MoonlightSession.hpp"
#include "Settings.hpp"
#include "features/performance/PerformanceLite.hpp"

#include <algorithm>
#include <fmt/format.h>

using namespace brls;

PerformanceTab::PerformanceTab() {
    inflateFromXMLRes("xml/views/ingame_overlay/performance_tab.xml");

    streamProfile->setText("Stream profile");
    network->setText("Configured bitrate");
    receiveLatency->setText("Receive latency");
    decodeLatency->setText("Decode latency");
    renderLatency->setText("Render latency");
    packetLoss->setText("Packet loss");
    renderedFps->setText("Rendered FPS");
    queueDepth->setText("Frame queue");
    refreshButton->setText("Refresh performance data");

    refreshButton->registerClickAction([this](View*) {
        refresh();
        return true;
    });

    refresh();
}

void PerformanceTab::refresh() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active()) {
        streamProfile->setDetailText("No active stream");
        network->setDetailText("-");
        receiveLatency->setDetailText("-");
        decodeLatency->setDetailText("-");
        renderLatency->setDetailText("-");
        packetLoss->setDetailText("-");
        renderedFps->setDetailText("-");
        queueDepth->setDetailText("-");
        return;
    }

    const auto* stats = session->session_stats();
    const auto& decode = stats->video_decode_stats;
    const auto& render = stats->video_render_stats;

    const uint64_t currentReceived = decode.total_received_frames;
    const uint64_t currentDropped = decode.network_dropped_frames;
    const uint64_t receivedDelta = currentReceived >= previousReceived
        ? currentReceived - previousReceived : 0;
    const uint64_t droppedDelta = currentDropped >= previousDropped
        ? currentDropped - previousDropped : 0;
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

    streamProfile->setDetailText(fmt::format("{}p @ {} FPS, {}",
        Settings::instance().resolution(), Settings::instance().fps(),
        getVideoCodecName(Settings::instance().video_codec())));
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
}

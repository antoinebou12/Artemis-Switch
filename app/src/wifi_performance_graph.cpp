#include "wifi_performance_graph.hpp"

#include <algorithm>

using namespace brls;

WifiPerformanceGraph::WifiPerformanceGraph() {
    setFocusable(false);
    setHideHighlightBackground(true);
    setHideHighlightBorder(true);
}

void WifiPerformanceGraph::pushSample(float normalizedSignal) {
    samples[nextSample] = std::clamp(normalizedSignal, 0.0f, 1.0f);
    nextSample = (nextSample + 1) % HISTORY_SIZE;
    sampleCount = std::min(sampleCount + 1, HISTORY_SIZE);
}

void WifiPerformanceGraph::clear() {
    samples.fill(0.0f);
    sampleCount = 0;
    nextSample = 0;
}

void WifiPerformanceGraph::draw(NVGcontext* vg, float x, float y, float width,
                                float height, Style style,
                                FrameContext* ctx) {
    Box::draw(vg, x, y, width, height, style, ctx);

    const float left = x + 12.0f;
    const float top = y + 10.0f;
    const float graphWidth = std::max(1.0f, width - 24.0f);
    const float graphHeight = std::max(1.0f, height - 20.0f);

    nvgSave(vg);
    nvgIntersectScissor(vg, x, y, width, height);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, left, top, graphWidth, graphHeight, 8.0f);
    nvgFillColor(vg, a(nvgRGBA(127, 127, 127, 20)));
    nvgFill(vg);

    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, a(nvgRGBA(127, 127, 127, 42)));
    for (int line = 1; line < 3; line++) {
        const float lineY = top + graphHeight * static_cast<float>(line) / 3.0f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, left, lineY);
        nvgLineTo(vg, left + graphWidth, lineY);
        nvgStroke(vg);
    }

    if (sampleCount > 0) {
        const size_t oldest = sampleCount == HISTORY_SIZE ? nextSample : 0;
        const float step = graphWidth /
            static_cast<float>(std::max<size_t>(HISTORY_SIZE - 1, 1));

        nvgBeginPath(vg);
        for (size_t index = 0; index < sampleCount; index++) {
            const float value = samples[(oldest + index) % HISTORY_SIZE];
            const float pointX = left + step * static_cast<float>(
                HISTORY_SIZE - sampleCount + index);
            const float pointY = top + graphHeight * (1.0f - value);
            if (index == 0)
                nvgMoveTo(vg, pointX, pointY);
            else
                nvgLineTo(vg, pointX, pointY);
        }
        nvgStrokeWidth(vg, 3.0f);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStrokeColor(vg, a(ctx->theme["brls/accent"]));
        nvgStroke(vg);

        const size_t latest = (nextSample + HISTORY_SIZE - 1) % HISTORY_SIZE;
        const float latestX = left + step * static_cast<float>(HISTORY_SIZE - 1);
        const float latestY = top + graphHeight * (1.0f - samples[latest]);
        nvgBeginPath(vg);
        nvgCircle(vg, latestX, latestY, 4.5f);
        nvgFillColor(vg, a(ctx->theme["brls/accent"]));
        nvgFill(vg);
    }

    nvgRestore(vg);
}

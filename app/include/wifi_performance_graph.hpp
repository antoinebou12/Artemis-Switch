#pragma once

#include <array>
#include <cstddef>

#include <borealis.hpp>

class WifiPerformanceGraph : public brls::Box {
public:
    WifiPerformanceGraph();

    static brls::View* create() { return new WifiPerformanceGraph(); }

    void pushSample(float normalizedSignal);
    void clear();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

private:
    static constexpr size_t HISTORY_SIZE = 30;

    std::array<float, HISTORY_SIZE> samples{};
    size_t sampleCount = 0;
    size_t nextSample = 0;
};

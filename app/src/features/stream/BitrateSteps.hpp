#pragma once

namespace artemis::stream {

// Inclusive kbps bounds of a bitrate slider.
struct BitrateSliderRange {
    int minKbps;
    int maxKbps;
};

// Platform bounds used by both the settings tab and the profile editor.
BitrateSliderRange bitrateSliderRange();

// Snaps a bitrate onto a step the user can actually reproduce: 0.5 Mbps below
// 20 Mbps, 1 Mbps above it. The raw slider resolves to one kbps per pixel,
// which is why a nudge used to land on values like 10347 kbps.
int quantizeBitrateKbps(int kbps, BitrateSliderRange range);

// Slider progress (0..1) -> quantized kbps, and back. Round-tripping a
// quantized bitrate through both returns the same value.
int bitrateFromSliderProgress(float progress, BitrateSliderRange range);
float sliderProgressFromBitrate(int kbps, BitrateSliderRange range);

} // namespace artemis::stream

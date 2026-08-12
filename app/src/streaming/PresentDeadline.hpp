#pragma once

#include <algorithm>
#include <cmath>

namespace artemis::streaming {

// Adaptive present-gate lead from measured Switch present cost.
// leadMs = clamp(renderP95 + gpuSubmitP95 + safety, minLead, maxLead)
struct PresentDeadlineParams {
    double safetyMs = 1.0;
    double minLeadMs = 1.5;
    double maxLeadMs = 8.0;
    double defaultLeadMs = 3.0;
    double jitterMultiplier = 1.5;
    double maxJitterMs = 8.0;
};

inline double clampPresentLeadMs(double renderP95Ms, double gpuSubmitP95Ms,
                                 bool havePresentSamples,
                                 const PresentDeadlineParams& p = {}) {
    if (!havePresentSamples ||
        (!std::isfinite(renderP95Ms) && !std::isfinite(gpuSubmitP95Ms))) {
        return p.defaultLeadMs;
    }
    const double render = std::isfinite(renderP95Ms) ? std::max(0.0, renderP95Ms) : 0.0;
    const double gpu =
        std::isfinite(gpuSubmitP95Ms) ? std::max(0.0, gpuSubmitP95Ms) : 0.0;
    return std::clamp(render + gpu + p.safetyMs, p.minLeadMs, p.maxLeadMs);
}

inline double presentGateLeadMs(double renderP95Ms, double gpuSubmitP95Ms,
                                bool havePresentSamples, double jitterMs,
                                const PresentDeadlineParams& p = {}) {
    const double base =
        clampPresentLeadMs(renderP95Ms, gpuSubmitP95Ms, havePresentSamples, p);
    const double jitter =
        std::clamp(std::isfinite(jitterMs) ? std::max(0.0, jitterMs) : 0.0, 0.0,
                   p.maxJitterMs);
    return base + p.jitterMultiplier * jitter;
}

} // namespace artemis::streaming

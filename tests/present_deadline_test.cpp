#include "PresentDeadline.hpp"

#include <cassert>
#include <cmath>

using artemis::streaming::PresentDeadlineParams;
using artemis::streaming::clampPresentLeadMs;
using artemis::streaming::presentGateLeadMs;

int main() {
    PresentDeadlineParams p;

    const double fallback = clampPresentLeadMs(0.0, 0.0, false, p);
    assert(std::abs(fallback - p.defaultLeadMs) < 1e-6);

    const double shortLead = clampPresentLeadMs(0.8, 0.7, true, p);
    assert(std::abs(shortLead - 2.5) < 1e-6); // 0.8+0.7+1.0

    const double clampedMin = clampPresentLeadMs(0.1, 0.1, true, p);
    assert(std::abs(clampedMin - p.minLeadMs) < 1e-6);

    const double clampedMax = clampPresentLeadMs(20.0, 20.0, true, p);
    assert(std::abs(clampedMax - p.maxLeadMs) < 1e-6);

    const double withJitter = presentGateLeadMs(0.8, 0.7, true, 2.0, p);
    assert(std::abs(withJitter - (2.5 + 1.5 * 2.0)) < 1e-6);

    const double cappedJitter = presentGateLeadMs(0.8, 0.7, true, 100.0, p);
    assert(std::abs(cappedJitter - (2.5 + 1.5 * p.maxJitterMs)) < 1e-6);

    return 0;
}

#include "../app/src/features/input/StickDeadzone.hpp"

#include <cassert>
#include <cmath>

using artemis::input::applyRadialDeadzone;
using artemis::input::StickAxes;

namespace {

bool near(float a, float b, float tolerance = 1e-4f) {
    return std::fabs(a - b) <= tolerance;
}

float magnitudeOf(StickAxes axes) {
    return std::sqrt(axes.x * axes.x + axes.y * axes.y);
}

} // namespace

int main() {
    // A deadzone of zero is the default and must be a pass-through, so nobody
    // who never configured one sees a behaviour change.
    for (float v = -1.0f; v <= 1.0f; v += 0.1f) {
        const StickAxes out = applyRadialDeadzone(v, 0.0f, 0.0f);
        assert(near(out.x, v));
        assert(near(out.y, 0.0f));
    }

    // Inside the deadzone still reads as centred.
    assert(near(magnitudeOf(applyRadialDeadzone(0.05f, 0.0f, 0.15f)), 0.0f));
    assert(near(magnitudeOf(applyRadialDeadzone(0.10f, 0.10f, 0.30f)), 0.0f));
    assert(near(magnitudeOf(applyRadialDeadzone(0.0f, 0.0f, 0.15f)), 0.0f));

    // Just past the threshold the output starts from zero rather than jumping
    // straight to the threshold value — this is the fix.
    const StickAxes justPast = applyRadialDeadzone(0.1501f, 0.0f, 0.15f);
    assert(magnitudeOf(justPast) < 0.01f);

    // Full deflection still reaches full range.
    assert(near(magnitudeOf(applyRadialDeadzone(1.0f, 0.0f, 0.15f)), 1.0f));
    assert(near(magnitudeOf(applyRadialDeadzone(0.0f, -1.0f, 0.30f)), 1.0f));

    // Halfway through the surviving band reads as half deflection.
    const StickAxes half = applyRadialDeadzone(0.55f, 0.0f, 0.10f);
    assert(near(magnitudeOf(half), 0.5f));

    // Direction is preserved, only magnitude is rescaled.
    const StickAxes diagonal = applyRadialDeadzone(0.6f, 0.8f, 0.20f);
    assert(near(diagonal.x / diagonal.y, 0.6f / 0.8f));
    assert(diagonal.x > 0.0f && diagonal.y > 0.0f);

    // Output is monotonic in input.
    float previous = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        const float in = i / 100.0f;
        const float out = magnitudeOf(applyRadialDeadzone(in, 0.0f, 0.25f));
        assert(out >= previous - 1e-5f);
        assert(out <= 1.0f + 1e-4f);
        previous = out;
    }

    // An absurd deadzone is capped instead of dividing by zero.
    const StickAxes capped = applyRadialDeadzone(1.0f, 0.0f, 1.0f);
    assert(std::isfinite(capped.x) && std::isfinite(capped.y));
    assert(near(magnitudeOf(capped), 1.0f));
    assert(near(magnitudeOf(applyRadialDeadzone(0.5f, 0.0f, 2.0f)), 0.0f));

    // Callers may supply their own magnitude approximation; an overestimate
    // must not push the result past full scale.
    const StickAxes approx = applyRadialDeadzone(1.0f, 0.0f, 1.02f, 0.15f);
    assert(magnitudeOf(approx) <= 1.0f + 1e-4f);

    return 0;
}

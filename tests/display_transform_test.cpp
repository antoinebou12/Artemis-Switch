#include "../app/src/features/video/DisplayTransform.hpp"

#include <cassert>
#include <cmath>
#include <array>

using namespace artemis::video;

bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

int main() {
    const NormalizedPoint original{0.2f, 0.7f};
    for (const auto rotation : std::array{Rotation::Deg0, Rotation::Deg90,
                                         Rotation::Deg180, Rotation::Deg270}) {
        const auto roundTrip = localToStream(streamToLocal(original, rotation), rotation);
        assert(near(roundTrip.x, original.x));
        assert(near(roundTrip.y, original.y));
    }
    const auto ninety = localToStream(original, Rotation::Deg90);
    assert(near(ninety.x, 0.7f) && near(ninety.y, 0.8f));
    const auto vector = localVectorToStream({2.0f, 3.0f}, Rotation::Deg270);
    assert(near(vector.x, -3.0f) && near(vector.y, 2.0f));
    assert(swapsDimensions(Rotation::Deg90));
    assert(nextRotation(Rotation::Deg270) == Rotation::Deg0);

    auto safe = validateDisplayTransform({Rotation::Deg0, 99.0f, 4.0f, -4.0f});
    assert(safe.zoom == 4.0f);
    assert(safe.panX == 0.75f && safe.panY == -0.75f);
}

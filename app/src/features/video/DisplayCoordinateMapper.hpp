#pragma once

#include "DisplayTransform.hpp"
#include "RendererPresentationPolicy.hpp"

#include <mutex>
#include <optional>

namespace artemis::video {

class DisplayCoordinateMapper {
public:
    static DisplayCoordinateMapper& instance();

    void update(int screenWidth, int screenHeight,
                const RendererPresentationPlan& plan, Rotation rotation);
    void reset();
    [[nodiscard]] std::optional<NormalizedPoint> localToStream(
        NormalizedPoint local) const;

private:
    mutable std::mutex m_mutex;
    int m_screenWidth = 0;
    int m_screenHeight = 0;
    RendererPresentationPlan m_plan;
    Rotation m_rotation = Rotation::Deg0;
};

} // namespace artemis::video

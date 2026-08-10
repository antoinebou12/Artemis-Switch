#include "DisplayCoordinateMapper.hpp"

#include <algorithm>

namespace artemis::video {

DisplayCoordinateMapper& DisplayCoordinateMapper::instance() {
    static DisplayCoordinateMapper mapper;
    return mapper;
}

void DisplayCoordinateMapper::update(int screenWidth, int screenHeight,
                                     const RendererPresentationPlan& plan,
                                     Rotation rotation) {
    std::scoped_lock lock(m_mutex);
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_plan = plan;
    m_rotation = rotation;
}

void DisplayCoordinateMapper::reset() {
    std::scoped_lock lock(m_mutex);
    m_screenWidth = m_screenHeight = 0;
    m_plan = {};
    m_rotation = Rotation::Deg0;
}

std::optional<NormalizedPoint> DisplayCoordinateMapper::localToStream(
    NormalizedPoint local) const {
    std::scoped_lock lock(m_mutex);
    const auto& destination = m_plan.geometry.destination;
    const auto& source = m_plan.geometry.source;
    if (m_screenWidth <= 0 || m_screenHeight <= 0 ||
        m_plan.logicalSourceWidth <= 0 || m_plan.logicalSourceHeight <= 0 ||
        destination.width <= 0 || destination.height <= 0)
        return std::nullopt;

    const float px = local.x * m_screenWidth;
    const float py = local.y * m_screenHeight;
    if (px < destination.x || py < destination.y ||
        px > destination.x + destination.width ||
        py > destination.y + destination.height)
        return std::nullopt;

    const float contentX = (px - destination.x) / destination.width;
    const float contentY = (py - destination.y) / destination.height;
    NormalizedPoint logical{
        (source.x + contentX * source.width) / m_plan.logicalSourceWidth,
        (source.y + contentY * source.height) / m_plan.logicalSourceHeight};
    auto mapped = artemis::video::localToStream(logical, m_rotation);
    mapped.x = std::clamp(mapped.x, 0.0f, 1.0f);
    mapped.y = std::clamp(mapped.y, 0.0f, 1.0f);
    return mapped;
}

} // namespace artemis::video

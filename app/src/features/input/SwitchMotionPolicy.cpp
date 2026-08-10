#include "SwitchMotionPolicy.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace artemis::input {

SwitchMotionCapabilities detectSwitchMotionCapabilities() {
    SwitchMotionCapabilities capabilities;

#ifdef __SWITCH__
    capabilities.documentedHandheldSixAxisAvailable = true;
#endif
    return capabilities;
}

bool canEnableConsoleMotionFallback(
    const SwitchMotionCapabilities& capabilities) {
    return capabilities.documentedHandheldSixAxisAvailable;
}

bool MotionFallbackGate::shouldForward(MotionSource source, uint64_t nowMs,
                                       bool handheld,
                                       const SwitchMotionOptions& options) {
    if (!options.allowGamepadMotionSensors)
        return false;

    if (source != MotionSource::Console) {
        m_lastControllerSampleMs = nowMs;
        m_hasControllerSample = true;
        return true;
    }

    if (!handheld || !options.allowConsoleMotionFallback)
        return false;
    if (!m_hasObservationStart) {
        m_observationStartMs = nowMs;
        m_hasObservationStart = true;
        return false;
    }
    if (nowMs < m_observationStartMs ||
        nowMs - m_observationStartMs < FALLBACK_DELAY_MS)
        return false;
    return !controllerRecentlyActive(nowMs);
}

void MotionFallbackGate::reset() {
    m_lastControllerSampleMs = 0;
    m_observationStartMs = 0;
    m_hasControllerSample = false;
    m_hasObservationStart = false;
}

bool MotionFallbackGate::controllerRecentlyActive(uint64_t nowMs) const {
    if (!m_hasControllerSample)
        return false;
    if (nowMs < m_lastControllerSampleMs)
        return true;
    return nowMs - m_lastControllerSampleMs < FALLBACK_DELAY_MS;
}

bool shouldForwardMotion(MotionSource source,
                         bool controllerReportsMotion,
                         const SwitchMotionOptions& options) {
    if (!options.allowGamepadMotionSensors)
        return false;

    if (source == MotionSource::Console)
        return options.allowConsoleMotionFallback && !controllerReportsMotion;

    return controllerReportsMotion;
}

} // namespace artemis::input

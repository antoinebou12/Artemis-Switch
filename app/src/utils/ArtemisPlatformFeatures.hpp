#pragma once

// Compile-time Artemis platform feature gates.
// Defaults are set from CMake (see ARTEMIS_* options in CMakeLists.txt).
// Switch-only behaviors must not end streams on desktop alt-tab / blur.

#ifndef ARTEMIS_END_STREAM_ON_FOCUS_LOSS
#if defined(PLATFORM_SWITCH)
#define ARTEMIS_END_STREAM_ON_FOCUS_LOSS 1
#else
#define ARTEMIS_END_STREAM_ON_FOCUS_LOSS 0
#endif
#endif

#ifndef ARTEMIS_SWITCH_RUNTIME_CLOCKS
#if defined(PLATFORM_SWITCH)
#define ARTEMIS_SWITCH_RUNTIME_CLOCKS 1
#else
#define ARTEMIS_SWITCH_RUNTIME_CLOCKS 0
#endif
#endif

#ifndef ARTEMIS_CLEAR_RUMBLE_ON_STREAM_START
#define ARTEMIS_CLEAR_RUMBLE_ON_STREAM_START 1
#endif

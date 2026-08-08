#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    if new in text:
        print(f"already applied: {path} -> {new.splitlines()[0][:70]}")
        return
    if old not in text:
        raise SystemExit(f"expected anchor not found in {path}:\n{old[:240]}")
    path.write_text(text.replace(old, new, 1))


def replace_all_exact(path: Path, old: str, new: str, expected: int) -> None:
    text = path.read_text()
    count = text.count(old)
    if count == 0 and text.count(new) >= expected:
        print(f"already applied: {path}")
        return
    if count != expected:
        raise SystemExit(
            f"expected {expected} occurrences in {path}, found {count}: {old[:160]}"
        )
    path.write_text(text.replace(old, new))


def patch_deko3d() -> None:
    path = ROOT / "app/src/streaming/video/deko3d/DKVideoRenderer.cpp"

    replace_once(
        path,
        '#include "Settings.hpp"\n#include <borealis/platforms/switch/switch_platform.hpp>',
        '#include "Settings.hpp"\n'
        '#include "../../../features/stream/AdvancedStreamOptionsStore.hpp"\n'
        '#include "../../../features/video/ZoomPanState.hpp"\n'
        '#include "../../../features/video/ZoomPanStore.hpp"\n'
        '#include "../../../video/VideoScale.hpp"\n'
        '#include "../../../video/VideoScaleStore.hpp"\n'
        '#include <borealis/platforms/switch/switch_platform.hpp>',
    )

    replace_once(
        path,
        'static void getFrameColorInfo(AVFrame* frame, AVColorSpace& colorSpace,\n'
        '                                  bool& colorFull) {\n'
        '        colorFull = frame->color_range == AVCOL_RANGE_JPEG;',
        'static void getFrameColorInfo(AVFrame* frame, AVColorSpace& colorSpace,\n'
        '                                  bool& colorFull, bool forceFullRange) {\n'
        '        colorFull = forceFullRange || frame->color_range == AVCOL_RANGE_JPEG;',
    )

    replace_once(
        path,
        '        if (frame->format == AV_PIX_FMT_NVTEGRA &&\n'
        '            frame->color_range == AVCOL_RANGE_JPEG) {',
        '        if (!forceFullRange && frame->format == AV_PIX_FMT_NVTEGRA &&\n'
        '            frame->color_range == AVCOL_RANGE_JPEG) {',
    )

    replace_all_exact(
        path,
        '    getFrameColorInfo(frame, colorSpace, colorFull);',
        '    const auto& artemisAdvancedOptions =\n'
        '        artemis::stream::AdvancedStreamOptionsStore::instance().get();\n'
        '    const bool forceFullRange = artemisAdvancedOptions.forceFullRangeVideo;\n'
        '    getFrameColorInfo(frame, colorSpace, colorFull, forceFullRange);',
        2,
    )

    replace_once(
        path,
        '    const SourceViewport sourceViewport = getSourceViewport(\n'
        '        m_frame_width, m_frame_height, m_screen_width, m_screen_height);\n'
        '    setTransformUvData(displayTransformState, sourceViewport, m_frame_width,\n'
        '                       m_frame_height);',
        '    const auto scaleMode = artemis::video::VideoScaleStore::instance().get();\n'
        '    const auto zoomPan = artemis::video::normalizeZoomPan(\n'
        '        artemis::video::ZoomPanStore::instance().get().state);\n'
        '    const auto presentation = artemis::video::VideoScale::presentationGeometry(\n'
        '        static_cast<float>(m_frame_width),\n'
        '        static_cast<float>(m_frame_height),\n'
        '        static_cast<float>(m_screen_width),\n'
        '        static_cast<float>(m_screen_height),\n'
        '        scaleMode, zoomPan.zoom, zoomPan.panX, zoomPan.panY);\n'
        '    const SourceViewport sourceViewport = {\n'
        '        presentation.source.x, presentation.source.y,\n'
        '        presentation.source.width, presentation.source.height};\n'
        '    const auto& destinationViewport = presentation.destination;\n'
        '    setTransformUvData(displayTransformState, sourceViewport, m_frame_width,\n'
        '                       m_frame_height);',
    )

    replace_once(
        path,
        '        EasuConstants easuConstants = {};\n'
        '        populateEasuConstants(easuConstants, sourceViewport, m_frame_width,\n'
        '                              m_frame_height, m_screen_width, m_screen_height);',
        '        EasuConstants easuConstants = {};\n'
        '        const int presentationWidth = std::max(\n'
        '            1, static_cast<int>(std::lround(destinationViewport.width)));\n'
        '        const int presentationHeight = std::max(\n'
        '            1, static_cast<int>(std::lround(destinationViewport.height)));\n'
        '        populateEasuConstants(easuConstants, sourceViewport, m_frame_width,\n'
        '                              m_frame_height, presentationWidth,\n'
        '                              presentationHeight);',
    )

    replace_once(
        path,
        '    auto drawDecodePass = [&](const Transformation& transformState) {',
        '    auto setPresentationViewport = [&](const artemis::video::RectF& rect) {\n'
        '        const float left = std::clamp(rect.x, 0.0f,\n'
        '                                      static_cast<float>(m_screen_width));\n'
        '        const float top = std::clamp(rect.y, 0.0f,\n'
        '                                     static_cast<float>(m_screen_height));\n'
        '        const float right = std::clamp(rect.x + rect.width, left,\n'
        '                                       static_cast<float>(m_screen_width));\n'
        '        const float bottom = std::clamp(rect.y + rect.height, top,\n'
        '                                        static_cast<float>(m_screen_height));\n'
        '        const float viewportWidth = std::max(1.0f, right - left);\n'
        '        const float viewportHeight = std::max(1.0f, bottom - top);\n'
        '        const uint32_t scissorX = static_cast<uint32_t>(std::floor(left));\n'
        '        const uint32_t scissorY = static_cast<uint32_t>(std::floor(top));\n'
        '        const uint32_t scissorRight = static_cast<uint32_t>(std::ceil(right));\n'
        '        const uint32_t scissorBottom = static_cast<uint32_t>(std::ceil(bottom));\n'
        '        cmdbuf.setViewports(\n'
        '            0, {{{left, top, viewportWidth, viewportHeight, 0.0f, 1.0f}}});\n'
        '        cmdbuf.setScissors(\n'
        '            0, {{{scissorX, scissorY,\n'
        '                  std::max(1u, scissorRight - scissorX),\n'
        '                  std::max(1u, scissorBottom - scissorY)}}});\n'
        '    };\n\n'
        '    auto preparePresentationViewport = [&]() {\n'
        '        cmdbuf.setViewports(\n'
        '            0, {{{0.0f, 0.0f, static_cast<float>(m_screen_width),\n'
        '                  static_cast<float>(m_screen_height), 0.0f, 1.0f}}});\n'
        '        cmdbuf.setScissors(\n'
        '            0, {{{0, 0, static_cast<uint32_t>(m_screen_width),\n'
        '                  static_cast<uint32_t>(m_screen_height)}}});\n'
        '        cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);\n'
        '        setPresentationViewport(destinationViewport);\n'
        '    };\n\n'
        '    auto drawDecodePass = [&](const Transformation& transformState) {',
    )

    replace_once(
        path,
        '        bindRenderTarget(upscalingTargetImage, m_upscaling_target_width,\n'
        '                         m_upscaling_target_height);\n'
        '        cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);\n'
        '        bindFullscreenState(upscalingFragmentShader);',
        '        bindRenderTarget(upscalingTargetImage, m_upscaling_target_width,\n'
        '                         m_upscaling_target_height);\n'
        '        preparePresentationViewport();\n'
        '        bindFullscreenState(upscalingFragmentShader);',
    )

    replace_once(
        path,
        '    } else if (useDithering || useRcas) {\n'
        '        bindRenderTarget(upscalingTargetImage, m_upscaling_target_width,\n'
        '                         m_upscaling_target_height);\n'
        '        drawDecodePass(displayTransformState);\n'
        '    } else {\n'
        '        drawDecodePass(displayTransformState);\n'
        '    }',
        '    } else if (useDithering || useRcas) {\n'
        '        bindRenderTarget(upscalingTargetImage, m_upscaling_target_width,\n'
        '                         m_upscaling_target_height);\n'
        '        preparePresentationViewport();\n'
        '        drawDecodePass(displayTransformState);\n'
        '    } else {\n'
        '        preparePresentationViewport();\n'
        '        drawDecodePass(displayTransformState);\n'
        '    }',
    )

    replace_once(
        path,
        '#else\n    drawDecodePass(displayTransformState);\n#endif\n    cmdlist = cmdbuf.finishList();',
        '#else\n    preparePresentationViewport();\n    drawDecodePass(displayTransformState);\n#endif\n    cmdlist = cmdbuf.finishList();',
    )

    replace_once(
        path,
        '#ifdef SUPPORT_UPSCALING\n    m_rcas_enabled = useRcas && rcasTargetHandle && rcasFragmentShader &&\n'
        '                     rcasUniformBuffer;\n#endif\n}',
        '#ifdef SUPPORT_UPSCALING\n    m_rcas_enabled = useRcas && rcasTargetHandle && rcasFragmentShader &&\n'
        '                     rcasUniformBuffer;\n#endif\n'
        '    m_artemis_scale_mode = static_cast<int>(scaleMode);\n'
        '    m_artemis_zoom = zoomPan.zoom;\n'
        '    m_artemis_pan_x = zoomPan.panX;\n'
        '    m_artemis_pan_y = zoomPan.panY;\n'
        '    m_artemis_force_full_range = forceFullRange;\n'
        '}',
    )

    replace_once(
        path,
        '    const bool colorChanged =\n'
        '        m_color_space != static_cast<int>(colorSpace) || m_color_full != colorFull;\n#ifdef SUPPORT_UPSCALING',
        '    const bool colorChanged =\n'
        '        m_color_space != static_cast<int>(colorSpace) || m_color_full != colorFull;\n'
        '    const auto artemisScaleMode =\n'
        '        artemis::video::VideoScaleStore::instance().get();\n'
        '    const auto artemisZoomPan = artemis::video::normalizeZoomPan(\n'
        '        artemis::video::ZoomPanStore::instance().get().state);\n'
        '    const bool artemisPresentationChanged =\n'
        '        m_artemis_scale_mode != static_cast<int>(artemisScaleMode) ||\n'
        '        std::fabs(m_artemis_zoom - artemisZoomPan.zoom) > 0.0001f ||\n'
        '        std::fabs(m_artemis_pan_x - artemisZoomPan.panX) > 0.0001f ||\n'
        '        std::fabs(m_artemis_pan_y - artemisZoomPan.panY) > 0.0001f;\n'
        '    const bool artemisFullRangeChanged =\n'
        '        m_artemis_force_full_range != forceFullRange;\n#ifdef SUPPORT_UPSCALING',
    )

    replace_once(
        path,
        '    if (!frameSizeChanged && !screenSizeChanged && !colorChanged &&\n'
        '        !ditheringChanged && !upscalingChanged && !rcasChanged) {',
        '    if (!frameSizeChanged && !screenSizeChanged && !colorChanged &&\n'
        '        !ditheringChanged && !upscalingChanged && !rcasChanged &&\n'
        '        !artemisPresentationChanged && !artemisFullRangeChanged) {',
    )


def patch_input() -> None:
    path = ROOT / "app/src/streaming/InputManager.cpp"

    replace_once(
        path,
        '#include "InputManager.hpp"\n#include "Limelight.h"\n#include "Settings.hpp"',
        '#include "InputManager.hpp"\n#include "Limelight.h"\n#include "Settings.hpp"\n'
        '#include "../features/input/SwitchMotionPolicy.hpp"\n'
        '#include "../features/input/SwitchMotionPolicyStore.hpp"',
    )

    replace_once(
        path,
        '        ->getControllerSensorStateChanged()\n'
        '        ->subscribe([this](brls::SensorEvent event) {\n'
        '            if (!inputEnabled) return;\n'
        '            \n'
        '            switch (event.type) {',
        '        ->getControllerSensorStateChanged()\n'
        '        ->subscribe([this](brls::SensorEvent event) {\n'
        '            if (!inputEnabled) return;\n\n'
        '            const auto& motionOptions =\n'
        '                artemis::input::SwitchMotionPolicyStore::instance().get();\n'
        '            if (!artemis::input::shouldForwardMotion(\n'
        '                    artemis::input::MotionSource::JoyCon, true,\n'
        '                    motionOptions)) {\n'
        '                return;\n'
        '            }\n\n'
        '            switch (event.type) {',
    )


if __name__ == "__main__":
    patch_deko3d()
    patch_input()
    print("Applied Artemis Switch runtime hooks")

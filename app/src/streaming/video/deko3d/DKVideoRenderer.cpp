#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)

#include "ArtemisDKVideoRenderer.hpp"

// Preload the dependencies used by the legacy source before temporarily
// exposing its private implementation. This avoids leaking the private/public
// macro into STL, Borealis, deko3d, or FFmpeg headers.
#include "IVideoRenderer.hpp"
#include "Settings.hpp"
#include <deko3d.hpp>
#include <glm/mat4x4.hpp>
#include <borealis.hpp>
#include <borealis/platforms/switch/switch_video.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>
#include <nanovg/framework/CCmdMemRing.h>
#include <nanovg/framework/CShader.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libavutil/imgutils.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

// Keep the upstream Moonlight-Switch deko3D renderer intact and reuse it as
// the compatibility/resource-management implementation. Artemis only takes
// over the presentation command list when a feature actually needs it.
#define private public
#define DKVideoRenderer LegacyDKVideoRenderer
#include "DKVideoRendererLegacy.inc"
#undef DKVideoRenderer
#undef private

#include "../../../features/stream/AdvancedStreamOptionsStore.hpp"
#include "../../../features/video/ZoomPanState.hpp"
#include "../../../features/video/ZoomPanStore.hpp"
#include "../../../video/VideoScale.hpp"
#include "../../../video/VideoScaleStore.hpp"

namespace {

using artemis::video::PresentationGeometry;
using artemis::video::RectF;
using artemis::video::ScaleMode;
using artemis::video::VideoScale;

bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) <= 0.0001f;
}

void populateColorTransform(Transformation& transform,
                            AVColorSpace colorSpace,
                            bool colorFull) {
    const glm::vec3 colorOffset = gl_color_offset(colorFull);
    const glm::mat3 colorMatrix = gl_color_matrix(colorSpace, colorFull);

    transform = {};
    transform.yuvmat_col0[0] = colorMatrix[0][0];
    transform.yuvmat_col0[1] = colorMatrix[0][1];
    transform.yuvmat_col0[2] = colorMatrix[0][2];

    transform.yuvmat_col1[0] = colorMatrix[1][0];
    transform.yuvmat_col1[1] = colorMatrix[1][1];
    transform.yuvmat_col1[2] = colorMatrix[1][2];

    transform.yuvmat_col2[0] = colorMatrix[2][0];
    transform.yuvmat_col2[1] = colorMatrix[2][1];
    transform.yuvmat_col2[2] = colorMatrix[2][2];

    transform.offset[0] = colorOffset[0];
    transform.offset[1] = colorOffset[1];
    transform.offset[2] = colorOffset[2];
}

void populateUvTransform(Transformation& transform,
                         const RectF& source,
                         int frameWidth,
                         int frameHeight) {
    if (frameWidth <= 0 || frameHeight <= 0 ||
        source.width <= 0.0f || source.height <= 0.0f) {
        transform.uv_data[0] = 0.0f;
        transform.uv_data[1] = 0.0f;
        transform.uv_data[2] = 1.0f;
        transform.uv_data[3] = 1.0f;
        return;
    }

    transform.uv_data[0] = source.x / static_cast<float>(frameWidth);
    transform.uv_data[1] = source.y / static_cast<float>(frameHeight);
    transform.uv_data[2] = static_cast<float>(frameWidth) / source.width;
    transform.uv_data[3] = static_cast<float>(frameHeight) / source.height;
}

} // namespace

class ArtemisDKVideoRenderer::Impl {
public:
    LegacyDKVideoRenderer legacy;

    bool customPathActive = false;
    int cachedFrameWidth = -1;
    int cachedFrameHeight = -1;
    int cachedScreenWidth = -1;
    int cachedScreenHeight = -1;
    int cachedColorSpace = -1;
    bool cachedColorFull = false;
    ScaleMode cachedScaleMode = ScaleMode::Fill;
    float cachedZoom = 1.0f;
    float cachedPanX = 0.0f;
    float cachedPanY = 0.0f;
    bool cachedForceFullRange = false;

    bool wantsCustomPresentation() const {
        const ScaleMode scaleMode = artemis::video::VideoScaleStore::instance().get();
        const auto zoomPan = artemis::video::normalizeZoomPan(
            artemis::video::ZoomPanStore::instance().get().state);
        const bool forceFullRange =
            artemis::stream::AdvancedStreamOptionsStore::instance()
                .get()
                .forceFullRangeVideo;

        return scaleMode != ScaleMode::Fill ||
               !nearlyEqual(zoomPan.zoom, 1.0f) ||
               !nearlyEqual(zoomPan.panX, 0.0f) ||
               !nearlyEqual(zoomPan.panY, 0.0f) ||
               forceFullRange;
    }

    void restoreLegacyCommands(int width, int height, AVFrame* frame) {
        if (!customPathActive || !legacy.m_is_initialized)
            return;

        legacy.queue.waitIdle();
        legacy.m_screen_width = width;
        legacy.m_screen_height = height;
        legacy.recordStaticCommands(frame);
        customPathActive = false;
    }

    bool customStateChanged(int width, int height, AVFrame* frame,
                            ScaleMode scaleMode,
                            const artemis::video::ZoomPanState& zoomPan,
                            bool forceFullRange,
                            AVColorSpace colorSpace,
                            bool colorFull) const {
        return !customPathActive ||
               cachedFrameWidth != frame->width ||
               cachedFrameHeight != frame->height ||
               cachedScreenWidth != width ||
               cachedScreenHeight != height ||
               cachedColorSpace != static_cast<int>(colorSpace) ||
               cachedColorFull != colorFull ||
               cachedScaleMode != scaleMode ||
               !nearlyEqual(cachedZoom, zoomPan.zoom) ||
               !nearlyEqual(cachedPanX, zoomPan.panX) ||
               !nearlyEqual(cachedPanY, zoomPan.panY) ||
               cachedForceFullRange != forceFullRange;
    }

    void prepareLegacyFrameState(int width, int height, AVFrame* frame) {
        const bool frameSizeChanged =
            legacy.m_frame_width != frame->width ||
            legacy.m_frame_height != frame->height;

        if (frameSizeChanged) {
            legacy.queue.waitIdle();
            legacy.m_frame_width = frame->width;
            legacy.m_frame_height = frame->height;
            legacy.frameMappings.clear();
            legacy.currentMappingIndex = -1;
            legacy.updateFrameLayouts();
        }

        legacy.m_screen_width = width;
        legacy.m_screen_height = height;
    }

    bool recordCustomCommands(int width, int height, AVFrame* frame,
                              ScaleMode scaleMode,
                              const artemis::video::ZoomPanState& zoomPan,
                              bool forceFullRange,
                              AVColorSpace colorSpace,
                              bool colorFull) {
        if (width <= 0 || height <= 0 || frame->width <= 0 || frame->height <= 0)
            return false;

        const PresentationGeometry geometry = VideoScale::presentationGeometry(
            static_cast<float>(frame->width),
            static_cast<float>(frame->height),
            static_cast<float>(width),
            static_cast<float>(height),
            scaleMode, zoomPan.zoom, zoomPan.panX, zoomPan.panY);

        if (geometry.source.width <= 0.0f || geometry.source.height <= 0.0f ||
            geometry.destination.width <= 0.0f ||
            geometry.destination.height <= 0.0f) {
            return false;
        }

        dk::Image* framebuffer = legacy.vctx->getFramebuffer();
        dk::Image* depthBuffer = legacy.vctx->getDepthBuffer();
        if (!framebuffer || !depthBuffer)
            return false;

        Transformation transform{};
        populateColorTransform(transform, colorSpace, colorFull);
        populateUvTransform(transform, geometry.source, frame->width, frame->height);

        dk::ImageView colorTarget{*framebuffer};
        dk::ImageView depthTarget{*depthBuffer};
        dk::RasterizerState rasterizerState;
        dk::DepthStencilState depthStencilState;
        dk::ColorState colorState;
        dk::ColorWriteState colorWriteState;

        legacy.cmdbuf.clear();
        legacy.cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
        legacy.cmdbuf.bindRasterizerState(rasterizerState);
        legacy.cmdbuf.bindDepthStencilState(
            depthStencilState.setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setStencilTestEnable(false));
        legacy.cmdbuf.bindColorState(colorState);
        legacy.cmdbuf.bindColorWriteState(colorWriteState);
        legacy.cmdbuf.bindShaders(DkStageFlag_GraphicsMask,
                                  {legacy.vertexShader, legacy.fragmentShader});
        legacy.cmdbuf.bindVtxBuffer(0, legacy.vertexBuffer.getGpuAddr(),
                                    legacy.vertexBuffer.getSize());
        legacy.cmdbuf.bindVtxAttribState(VertexAttribState);
        legacy.cmdbuf.bindVtxBufferState(VertexBufferState);
        legacy.cmdbuf.bindTextures(DkStage_Fragment, 0,
                                   dkMakeTextureHandle(legacy.lumaTextureId, 0));
        legacy.cmdbuf.bindTextures(DkStage_Fragment, 1,
                                   dkMakeTextureHandle(legacy.chromaTextureId, 0));
        legacy.cmdbuf.bindUniformBuffer(DkStage_Fragment, 0,
                                        legacy.transformUniformBuffer.getGpuAddr(),
                                        legacy.transformUniformBuffer.getSize());
        legacy.cmdbuf.pushConstants(legacy.transformUniformBuffer.getGpuAddr(),
                                    legacy.transformUniformBuffer.getSize(), 0,
                                    sizeof(transform), &transform);

        // Clear the entire Switch framebuffer first so Fit mode gets true black
        // letterbox/pillarbox bars rather than stale pixels from the previous frame.
        legacy.cmdbuf.setViewports(
            0, {{{0.0f, 0.0f, static_cast<float>(width),
                  static_cast<float>(height), 0.0f, 1.0f}}});
        legacy.cmdbuf.setScissors(
            0, {{{0, 0, static_cast<uint32_t>(width),
                  static_cast<uint32_t>(height)}}});
        legacy.cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);

        const float left = std::clamp(geometry.destination.x, 0.0f,
                                      static_cast<float>(width));
        const float top = std::clamp(geometry.destination.y, 0.0f,
                                     static_cast<float>(height));
        const float right = std::clamp(
            geometry.destination.x + geometry.destination.width,
            left, static_cast<float>(width));
        const float bottom = std::clamp(
            geometry.destination.y + geometry.destination.height,
            top, static_cast<float>(height));

        const float viewportWidth = std::max(1.0f, right - left);
        const float viewportHeight = std::max(1.0f, bottom - top);
        const uint32_t scissorX = static_cast<uint32_t>(std::floor(left));
        const uint32_t scissorY = static_cast<uint32_t>(std::floor(top));
        const uint32_t scissorRight = static_cast<uint32_t>(std::ceil(right));
        const uint32_t scissorBottom = static_cast<uint32_t>(std::ceil(bottom));

        legacy.cmdbuf.setViewports(
            0, {{{left, top, viewportWidth, viewportHeight, 0.0f, 1.0f}}});
        legacy.cmdbuf.setScissors(
            0, {{{scissorX, scissorY,
                  std::max(1u, scissorRight - scissorX),
                  std::max(1u, scissorBottom - scissorY)}}});
        legacy.cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
        legacy.cmdlist = legacy.cmdbuf.finishList();

        // The custom presentation path is deliberately direct for the first
        // integration milestone. Default Fill continues to use the untouched
        // legacy FSR/RCAS/dithering path. We can move the same geometry into
        // the post-processing pass after real Switch validation.
        legacy.m_dithering_enabled = false;
        legacy.m_upscaling_enabled = false;
#ifdef SUPPORT_UPSCALING
        legacy.m_rcas_enabled = false;
#endif

        cachedFrameWidth = frame->width;
        cachedFrameHeight = frame->height;
        cachedScreenWidth = width;
        cachedScreenHeight = height;
        cachedColorSpace = static_cast<int>(colorSpace);
        cachedColorFull = colorFull;
        cachedScaleMode = scaleMode;
        cachedZoom = zoomPan.zoom;
        cachedPanX = zoomPan.panX;
        cachedPanY = zoomPan.panY;
        cachedForceFullRange = forceFullRange;
        customPathActive = true;
        return true;
    }

    void updateDirectRenderStats(uint64_t beforeRender) {
        const uint64_t renderTime = LiGetMillis() - beforeRender;
        legacy.m_video_render_stats_progress.total_render_time += renderTime;
        legacy.m_video_render_stats_progress.rendered_frames++;

        constexpr uint64_t StatsIntervalMs = 200;
        const uint64_t now = LiGetMillis();
        if (now - legacy.m_video_render_stats_progress.measurement_start_timestamp <
            StatsIntervalMs) {
            return;
        }

        legacy.m_video_render_stats_cache = legacy.m_video_render_stats_progress;
        legacy.m_video_render_stats_progress = {};

        const uint64_t elapsed =
            now - legacy.m_video_render_stats_cache.measurement_start_timestamp;
        legacy.m_video_render_stats_cache.rendered_fps =
            elapsed && legacy.m_video_render_stats_cache.rendered_frames > 1
                ? static_cast<float>(
                      legacy.m_video_render_stats_cache.rendered_frames - 1) /
                      (static_cast<float>(elapsed) / 1000.0f)
                : 0.0f;
        legacy.m_video_render_stats_cache.rendering_time =
            legacy.m_video_render_stats_cache.rendered_frames
                ? static_cast<float>(
                      legacy.m_video_render_stats_cache.total_render_time) /
                      static_cast<float>(
                          legacy.m_video_render_stats_cache.rendered_frames)
                : 0.0f;
    }

    void drawCustom(NVGcontext* vg, int width, int height, AVFrame* frame,
                    int imageFormat) {
        (void)vg;
        (void)imageFormat;

        legacy.checkAndInitialize(width, height, frame);
        if (!legacy.m_is_initialized)
            return;

        const uint64_t beforeRender = LiGetMillis();
        if (!legacy.m_video_render_stats_progress.rendered_frames) {
            legacy.m_video_render_stats_progress.measurement_start_timestamp =
                beforeRender;
        }

        prepareLegacyFrameState(width, height, frame);

        const ScaleMode scaleMode = artemis::video::VideoScaleStore::instance().get();
        const auto zoomPan = artemis::video::normalizeZoomPan(
            artemis::video::ZoomPanStore::instance().get().state);
        const bool forceFullRange =
            artemis::stream::AdvancedStreamOptionsStore::instance()
                .get()
                .forceFullRangeVideo;

        AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
        bool colorFull = false;
        getFrameColorInfo(frame, colorSpace, colorFull);
        if (forceFullRange)
            colorFull = true;

        if (customStateChanged(width, height, frame, scaleMode, zoomPan,
                               forceFullRange, colorSpace, colorFull)) {
            legacy.queue.waitIdle();
            if (!recordCustomCommands(width, height, frame, scaleMode, zoomPan,
                                      forceFullRange, colorSpace, colorFull)) {
                brls::Logger::warning(
                    "Artemis deko3D presentation setup failed; falling back to legacy renderer");
                customPathActive = false;
                legacy.recordStaticCommands(frame);
                legacy.draw(vg, width, height, frame, imageFormat);
                return;
            }
        }

        legacy.updateFrameMapping(frame);
        if (legacy.cmdlist != 0)
            legacy.queue.submitCommands(legacy.cmdlist);

        updateDirectRenderStats(beforeRender);
    }
};

ArtemisDKVideoRenderer::ArtemisDKVideoRenderer()
    : impl(std::make_unique<Impl>()) {}

ArtemisDKVideoRenderer::~ArtemisDKVideoRenderer() = default;

void ArtemisDKVideoRenderer::draw(NVGcontext* vg, int width, int height,
                                   AVFrame* frame, int imageFormat) {
    if (!frame)
        return;

    if (!impl->wantsCustomPresentation()) {
        impl->restoreLegacyCommands(width, height, frame);
        impl->legacy.draw(vg, width, height, frame, imageFormat);
        return;
    }

    impl->drawCustom(vg, width, height, frame, imageFormat);
}

VideoRenderStats* ArtemisDKVideoRenderer::video_render_stats() {
    return impl->legacy.video_render_stats();
}

#endif // PLATFORM_SWITCH && BOREALIS_USE_DEKO3D

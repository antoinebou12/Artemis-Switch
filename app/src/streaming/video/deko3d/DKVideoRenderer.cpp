#if defined(PLATFORM_SWITCH) && defined(BOREALIS_USE_DEKO3D)

#include "ArtemisDKVideoRenderer.hpp"

#include "IVideoRenderer.hpp"
#include "Settings.hpp"
#include <borealis.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>
#include <borealis/platforms/switch/switch_video.hpp>
#include <deko3d.hpp>
#include <glm/mat4x4.hpp>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_nvtegra.h>
#include <libavutil/imgutils.h>
#include <nanovg/framework/CCmdMemRing.h>
#include <nanovg/framework/CShader.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

// Reuse the proven Moonlight-Switch decoder mappings, descriptor handling,
// FSR/RCAS/dithering resources, and statistics. artemi-switch owns only the
// final presentation geometry so Fit/Fill/Stretch never switch renderer paths.
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
    if (frameWidth <= 0 || frameHeight <= 0 || source.width <= 0.0f ||
        source.height <= 0.0f) {
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

RectF clampDestination(const RectF& destination, int width, int height) {
    const float left = std::clamp(destination.x, 0.0f, static_cast<float>(width));
    const float top = std::clamp(destination.y, 0.0f, static_cast<float>(height));
    const float right = std::clamp(destination.x + destination.width, left,
                                   static_cast<float>(width));
    const float bottom = std::clamp(destination.y + destination.height, top,
                                    static_cast<float>(height));
    return {left, top, std::max(1.0f, right - left),
            std::max(1.0f, bottom - top)};
}
} // namespace

class ArtemisDKVideoRenderer::Impl {
public:
    LegacyDKVideoRenderer legacy;

    bool presentationReady = false;
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
    bool cachedDithering = false;
    bool cachedUpscaling = false;
    bool cachedRcas = false;

    void prepareFrameState(int width, int height, AVFrame* frame) {
        const bool frameSizeChanged = legacy.m_frame_width != frame->width ||
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

    bool stateChanged(int width, int height, AVFrame* frame,
                      ScaleMode scaleMode,
                      const artemis::video::ZoomPanState& zoomPan,
                      bool forceFullRange,
                      AVColorSpace colorSpace,
                      bool colorFull) const {
        const bool dithering = Settings::instance().dithering();
        const bool upscaling = Settings::instance().upscaling();
        const bool rcas = Settings::instance().rcas();
        return !presentationReady || cachedFrameWidth != frame->width ||
               cachedFrameHeight != frame->height || cachedScreenWidth != width ||
               cachedScreenHeight != height ||
               cachedColorSpace != static_cast<int>(colorSpace) ||
               cachedColorFull != colorFull || cachedScaleMode != scaleMode ||
               !nearlyEqual(cachedZoom, zoomPan.zoom) ||
               !nearlyEqual(cachedPanX, zoomPan.panX) ||
               !nearlyEqual(cachedPanY, zoomPan.panY) ||
               cachedForceFullRange != forceFullRange ||
               cachedDithering != dithering || cachedUpscaling != upscaling ||
               cachedRcas != rcas;
    }

    bool recordPresentation(int width, int height, AVFrame* frame,
                            ScaleMode scaleMode,
                            const artemis::video::ZoomPanState& zoomPan,
                            bool forceFullRange,
                            AVColorSpace colorSpace,
                            bool colorFull) {
        if (width <= 0 || height <= 0 || frame->width <= 0 || frame->height <= 0)
            return false;

        const PresentationGeometry geometry = VideoScale::presentationGeometry(
            static_cast<float>(frame->width), static_cast<float>(frame->height),
            static_cast<float>(width), static_cast<float>(height), scaleMode,
            zoomPan.zoom, zoomPan.panX, zoomPan.panY);
        if (geometry.source.width <= 0.0f || geometry.source.height <= 0.0f ||
            geometry.destination.width <= 0.0f ||
            geometry.destination.height <= 0.0f)
            return false;

        const RectF destination = clampDestination(geometry.destination, width, height);

        bool useDithering = false;
        bool useUpscaling = false;
        bool useRcas = false;
#ifdef SUPPORT_UPSCALING
        useDithering = Settings::instance().dithering() &&
                       Settings::instance().dithering_strength() > 0.0f;
        // Reuse the legacy FSR eligibility rule because ensureUpscalingResources()
        // allocates the FSR source image from that exact rule. Zoom/Pan can make
        // the visible source smaller than the output, but that must not create a
        // half-enabled FSR state without a source texture.
        useUpscaling = Settings::instance().upscaling() && legacy.shouldUseUpscaling();
        useRcas = Settings::instance().rcas() &&
                  Settings::instance().rcas_strength() > 0.0f;

        // Allocate the existing Moonlight post-process resources once. If the
        // GPU allocation fails, keep streaming with the same presentation
        // geometry on the direct path instead of changing scale modes.
        if ((useDithering || useUpscaling || useRcas) &&
            !legacy.ensureUpscalingResources()) {
            brls::Logger::warning(
                "artemi-switch: video filtering unavailable, using direct presentation");
            useDithering = false;
            useUpscaling = false;
            useRcas = false;
        }

        // Defensive guard: never report FSR active unless every resource needed
        // by the EASU pass exists. This keeps Zoom/Pan and mode switches stable.
        if (useUpscaling &&
            (!legacy.sourceTargetHandle || !legacy.upscalingTargetHandle ||
             legacy.sourceTextureId < 0 || !legacy.upscalingFragmentShader ||
             !legacy.easuUniformBuffer)) {
            brls::Logger::warning(
                "artemi-switch: incomplete FSR resources, disabling FSR for this presentation state");
            useUpscaling = false;
        }
#endif

        dk::Image* framebuffer = legacy.vctx->getFramebuffer();
        dk::Image* depthBuffer = legacy.vctx->getDepthBuffer();
        if (!framebuffer || !depthBuffer)
            return false;

        Transformation displayTransform{};
        populateColorTransform(displayTransform, colorSpace, colorFull);
        populateUvTransform(displayTransform, geometry.source, frame->width,
                            frame->height);

        Transformation sourceTransform = displayTransform;
        populateUvTransform(sourceTransform,
                            {0.0f, 0.0f, static_cast<float>(frame->width),
                             static_cast<float>(frame->height)},
                            frame->width, frame->height);

        legacy.cmdbuf.clear();
        dk::RasterizerState rasterizerState;
        dk::DepthStencilState depthStencilState;
        dk::ColorState colorState;
        dk::ColorWriteState colorWriteState;

        auto bindCommonState = [&](const CShader& fragmentShader) {
            legacy.cmdbuf.bindShaders(DkStageFlag_GraphicsMask,
                                      {legacy.vertexShader, fragmentShader});
            legacy.cmdbuf.bindRasterizerState(rasterizerState);
            legacy.cmdbuf.bindDepthStencilState(
                depthStencilState.setDepthTestEnable(false)
                    .setDepthWriteEnable(false)
                    .setStencilTestEnable(false));
            legacy.cmdbuf.bindColorState(colorState);
            legacy.cmdbuf.bindColorWriteState(colorWriteState);
            legacy.cmdbuf.bindVtxBuffer(0, legacy.vertexBuffer.getGpuAddr(),
                                        legacy.vertexBuffer.getSize());
            legacy.cmdbuf.bindVtxAttribState(VertexAttribState);
            legacy.cmdbuf.bindVtxBufferState(VertexBufferState);
        };

        auto setViewport = [&](const RectF& rect) {
            const uint32_t x = static_cast<uint32_t>(std::floor(rect.x));
            const uint32_t y = static_cast<uint32_t>(std::floor(rect.y));
            const uint32_t right = static_cast<uint32_t>(std::ceil(rect.x + rect.width));
            const uint32_t bottom = static_cast<uint32_t>(std::ceil(rect.y + rect.height));
            legacy.cmdbuf.setViewports(
                0, {{{rect.x, rect.y, rect.width, rect.height, 0.0f, 1.0f}}});
            legacy.cmdbuf.setScissors(
                0, {{{x, y, std::max(1u, right - x), std::max(1u, bottom - y)}}});
        };

        auto setFullViewport = [&](int targetWidth, int targetHeight) {
            setViewport({0.0f, 0.0f, static_cast<float>(targetWidth),
                         static_cast<float>(targetHeight)});
        };

        auto drawDecode = [&](const Transformation& transform) {
            bindCommonState(legacy.fragmentShader);
            legacy.cmdbuf.bindTextures(
                DkStage_Fragment, 0,
                dkMakeTextureHandle(legacy.lumaTextureId, 0));
            legacy.cmdbuf.bindTextures(
                DkStage_Fragment, 1,
                dkMakeTextureHandle(legacy.chromaTextureId, 0));
            legacy.cmdbuf.bindUniformBuffer(
                DkStage_Fragment, 0,
                legacy.transformUniformBuffer.getGpuAddr(),
                legacy.transformUniformBuffer.getSize());
            legacy.cmdbuf.pushConstants(
                legacy.transformUniformBuffer.getGpuAddr(),
                legacy.transformUniformBuffer.getSize(), 0, sizeof(transform),
                &transform);
            legacy.cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
        };

#ifdef SUPPORT_UPSCALING
        if (useUpscaling) {
            dk::ImageView sourceTarget{legacy.sourceTargetImage};
            legacy.cmdbuf.bindRenderTargets(&sourceTarget);
            setFullViewport(frame->width, frame->height);
            legacy.cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f,
                                     1.0f);
            drawDecode(sourceTransform);
            legacy.cmdbuf.barrier(DkBarrier_Tiles, DkInvalidateFlags_Image);

            EasuConstants easuConstants{};
            const SourceViewport sourceViewport{
                geometry.source.x, geometry.source.y, geometry.source.width,
                geometry.source.height};
            populateEasuConstants(
                easuConstants, sourceViewport, frame->width, frame->height,
                std::max(1, static_cast<int>(std::lround(destination.width))),
                std::max(1, static_cast<int>(std::lround(destination.height))));
            memcpy(legacy.easuUniformBuffer.getCpuAddr(), &easuConstants,
                   sizeof(easuConstants));

            dk::ImageView filteredTarget{legacy.upscalingTargetImage};
            legacy.cmdbuf.bindRenderTargets(&filteredTarget);
            setFullViewport(width, height);
            legacy.cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f,
                                     1.0f);
            setViewport(destination);
            bindCommonState(legacy.upscalingFragmentShader);
            legacy.cmdbuf.bindTextures(
                DkStage_Fragment, 0,
                dkMakeTextureHandle(legacy.sourceTextureId, 0));
            legacy.cmdbuf.bindUniformBuffer(
                DkStage_Fragment, 0, legacy.easuUniformBuffer.getGpuAddr(),
                legacy.easuUniformBuffer.getSize());
            legacy.cmdbuf.draw(DkPrimitive_Quads, QuadVertexData.size(), 1, 0, 0);
            legacy.cmdbuf.barrier(DkBarrier_Tiles, DkInvalidateFlags_Image);
        } else if ((useDithering || useRcas) && legacy.upscalingTargetHandle) {
            dk::ImageView filteredTarget{legacy.upscalingTargetImage};
            legacy.cmdbuf.bindRenderTargets(&filteredTarget);
            setFullViewport(width, height);
            legacy.cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f,
                                     1.0f);
            setViewport(destination);
            drawDecode(displayTransform);
            legacy.cmdbuf.barrier(DkBarrier_Tiles, DkInvalidateFlags_Image);
        } else
#endif
        {
            dk::ImageView colorTarget{*framebuffer};
            dk::ImageView depthTarget{*depthBuffer};
            legacy.cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
            setFullViewport(width, height);
            // Clear every frame. Fit therefore gets stable black bars and a
            // mode switch cannot expose pixels from the previous geometry.
            legacy.cmdbuf.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f,
                                     1.0f);
            setViewport(destination);
            drawDecode(displayTransform);
        }

        legacy.cmdlist = legacy.cmdbuf.finishList();
        legacy.m_color_space = static_cast<int>(colorSpace);
        legacy.m_color_full = colorFull;
        legacy.m_dithering_enabled = useDithering;
        legacy.m_upscaling_enabled = useUpscaling;
#ifdef SUPPORT_UPSCALING
        legacy.m_rcas_enabled = useRcas && legacy.rcasTargetHandle &&
                                legacy.rcasFragmentShader &&
                                legacy.rcasUniformBuffer;
        legacy.m_dithering_requested = Settings::instance().dithering();
        legacy.m_upscaling_requested = Settings::instance().upscaling();
        legacy.m_rcas_requested = Settings::instance().rcas();
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
        cachedDithering = Settings::instance().dithering();
        cachedUpscaling = Settings::instance().upscaling();
        cachedRcas = Settings::instance().rcas();
        presentationReady = true;
        return true;
    }

    void finishStats(uint64_t beforeRender) {
        const uint64_t renderTime = LiGetMillis() - beforeRender;
        legacy.m_video_render_stats_progress.total_render_time += renderTime;
        legacy.m_video_render_stats_progress.rendered_frames++;

        constexpr uint64_t statsIntervalMs = 200;
        const uint64_t now = LiGetMillis();
        if (now - legacy.m_video_render_stats_progress.measurement_start_timestamp <
            statsIntervalMs)
            return;

        legacy.m_video_render_stats_cache = legacy.m_video_render_stats_progress;
        legacy.m_video_render_stats_progress = {};

        const uint64_t elapsed =
            now - legacy.m_video_render_stats_cache.measurement_start_timestamp;
        legacy.m_video_render_stats_cache.rendered_fps =
            elapsed && legacy.m_video_render_stats_cache.rendered_frames > 1
                ? static_cast<float>(legacy.m_video_render_stats_cache.rendered_frames - 1) /
                      (static_cast<float>(elapsed) / 1000.0f)
                : 0.0f;
        legacy.m_video_render_stats_cache.rendering_time =
            legacy.m_video_render_stats_cache.rendered_frames
                ? static_cast<float>(legacy.m_video_render_stats_cache.total_render_time) /
                      static_cast<float>(legacy.m_video_render_stats_cache.rendered_frames)
                : 0.0f;
        legacy.m_video_render_stats_cache.post_processing_time =
            legacy.m_video_render_stats_cache.post_processed_frames
                ? (static_cast<float>(legacy.m_video_render_stats_cache.total_post_process_time) /
                   static_cast<float>(legacy.m_video_render_stats_cache.post_processed_frames)) /
                      1000.0f
                : 0.0f;
        legacy.m_video_render_stats_cache.dithering_time =
            legacy.m_video_render_stats_cache.dithered_frames
                ? (static_cast<float>(legacy.m_video_render_stats_cache.total_dithering_time) /
                   static_cast<float>(legacy.m_video_render_stats_cache.dithered_frames)) /
                      1000.0f
                : 0.0f;
        legacy.m_video_render_stats_cache.upscaling_time =
            legacy.m_video_render_stats_cache.upscaled_frames
                ? (static_cast<float>(legacy.m_video_render_stats_cache.total_upscaling_time) /
                   static_cast<float>(legacy.m_video_render_stats_cache.upscaled_frames)) /
                      1000.0f
                : 0.0f;
        legacy.m_video_render_stats_cache.sharpening_time =
            legacy.m_video_render_stats_cache.sharpened_frames
                ? (static_cast<float>(legacy.m_video_render_stats_cache.total_sharpening_time) /
                   static_cast<float>(legacy.m_video_render_stats_cache.sharpened_frames)) /
                      1000.0f
                : 0.0f;
    }

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame,
              int imageFormat) {
        (void)vg;
        (void)imageFormat;
        legacy.checkAndInitialize(width, height, frame);
        if (!legacy.m_is_initialized)
            return;

        const uint64_t beforeRender = LiGetMillis();
        if (!legacy.m_video_render_stats_progress.rendered_frames)
            legacy.m_video_render_stats_progress.measurement_start_timestamp =
                beforeRender;

        prepareFrameState(width, height, frame);
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

        if (stateChanged(width, height, frame, scaleMode, zoomPan,
                         forceFullRange, colorSpace, colorFull)) {
            legacy.queue.waitIdle();
            if (!recordPresentation(width, height, frame, scaleMode, zoomPan,
                                    forceFullRange, colorSpace, colorFull)) {
                brls::Logger::warning(
                    "artemi-switch: presentation setup failed, using Moonlight fallback");
                presentationReady = false;
                legacy.recordStaticCommands(frame);
                legacy.draw(vg, width, height, frame, imageFormat);
                return;
            }
        }

        legacy.updateFrameMapping(frame);
        if (legacy.cmdlist != 0) {
#ifdef SUPPORT_UPSCALING
            if (legacy.m_dithering_enabled || legacy.m_upscaling_enabled ||
                legacy.m_rcas_enabled) {
                const auto postProcessStart = PostProcessClock::now();
                const auto upscalingStart = PostProcessClock::now();
                legacy.queue.submitCommands(legacy.cmdlist);
                legacy.vctx->queueSignalFence(&legacy.upscalingFence);
                legacy.vctx->queueWaitFence(&legacy.upscalingFence);

                if (legacy.m_upscaling_enabled) {
                    legacy.m_video_render_stats_progress.total_upscaling_time +=
                        toMicroseconds(PostProcessClock::now() - upscalingStart);
                    legacy.m_video_render_stats_progress.upscaled_frames++;
                }

                const bool submitted = legacy.submitUpscalingPresentPass();
                legacy.vctx->queueFlush();
                if (submitted) {
                    legacy.m_video_render_stats_progress.total_post_process_time +=
                        toMicroseconds(PostProcessClock::now() - postProcessStart);
                    legacy.m_video_render_stats_progress.post_processed_frames++;
                }
            } else {
                legacy.queue.submitCommands(legacy.cmdlist);
            }
#else
            legacy.queue.submitCommands(legacy.cmdlist);
#endif
        }

        finishStats(beforeRender);
    }
};

ArtemisDKVideoRenderer::ArtemisDKVideoRenderer()
    : impl(std::make_unique<Impl>()) {}

ArtemisDKVideoRenderer::~ArtemisDKVideoRenderer() = default;

void ArtemisDKVideoRenderer::draw(NVGcontext* vg, int width, int height,
                                   AVFrame* frame, int imageFormat) {
    if (frame)
        impl->draw(vg, width, height, frame, imageFormat);
}

VideoRenderStats* ArtemisDKVideoRenderer::video_render_stats() {
    return impl->legacy.video_render_stats();
}

#endif // PLATFORM_SWITCH && BOREALIS_USE_DEKO3D

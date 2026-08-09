#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)

#pragma once

#include "IVideoRenderer.hpp"
#include <memory>

class ArtemisDKVideoRenderer final : public IVideoRenderer {
public:
    ArtemisDKVideoRenderer();
    ~ArtemisDKVideoRenderer() override;

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame,
              int imageFormat) override;
    VideoRenderStats* video_render_stats() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

#endif

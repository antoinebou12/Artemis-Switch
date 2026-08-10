#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

namespace artemis::ui {

// Renders a QR matrix with NanoVG. Used for host web-config URLs.
class QrCodeView : public brls::View {
  public:
    explicit QrCodeView(std::string payload);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    [[nodiscard]] bool valid() const { return !modules_.empty(); }
    [[nodiscard]] int size() const { return size_; }

  private:
    std::vector<bool> modules_;
    int size_ = 0;
};

void showUrlQrDialog(const std::string& title, const std::string& url);

} // namespace artemis::ui

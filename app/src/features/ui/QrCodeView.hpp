#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace artemis::ui {

// Renders a QR matrix with NanoVG (switch-wifi batched-path approach).
class QrCodeView : public brls::View {
  public:
    explicit QrCodeView(const std::string& payload);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    [[nodiscard]] bool valid() const { return !modules_.empty() && size_ > 0; }
    [[nodiscard]] int size() const { return size_; }

  private:
    int size_ = 0;
    std::vector<uint8_t> modules_;
};

void showUrlQrDialog(const std::string& title, const std::string& url);

} // namespace artemis::ui

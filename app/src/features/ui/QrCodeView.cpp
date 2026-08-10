#include "QrCodeView.hpp"

#include "qrcodegen.hpp"

#include <algorithm>
#include <cmath>

using namespace brls::literals;

namespace artemis::ui {

QrCodeView::QrCodeView(std::string payload) {
    try {
        const auto qr = qrcodegen::QrCode::encodeText(
            payload.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        size_ = qr.getSize();
        modules_.assign(static_cast<size_t>(size_ * size_), false);
        for (int y = 0; y < size_; ++y) {
            for (int x = 0; x < size_; ++x) {
                modules_[static_cast<size_t>(y * size_ + x)] = qr.getModule(x, y);
            }
        }
    } catch (...) {
        modules_.clear();
        size_ = 0;
    }
}

void QrCodeView::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style, brls::FrameContext*) {
    if (!valid() || size_ <= 0)
        return;

    const float side = std::min(width, height);
    const float module = side / static_cast<float>(size_ + 2);
    const float originX = x + (width - side) * 0.5f + module;
    const float originY = y + (height - side) * 0.5f + module;

    nvgBeginPath(vg);
    nvgRect(vg, x + (width - side) * 0.5f, y + (height - side) * 0.5f, side, side);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);

    nvgFillColor(vg, nvgRGB(0, 0, 0));
    for (int row = 0; row < size_; ++row) {
        for (int col = 0; col < size_; ++col) {
            if (!modules_[static_cast<size_t>(row * size_ + col)])
                continue;
            nvgBeginPath(vg);
            nvgRect(vg, originX + col * module, originY + row * module, module,
                    module);
            nvgFill(vg);
        }
    }
}

void showUrlQrDialog(const std::string& title, const std::string& url) {
    auto* holder = new brls::Box(brls::Axis::COLUMN);
    holder->setAlignItems(brls::AlignItems::CENTER);
    holder->setJustifyContent(brls::JustifyContent::CENTER);
    holder->setPadding(20);

    auto* heading = new brls::Label();
    heading->setText(title);
    heading->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    heading->setMarginBottom(12);
    holder->addView(heading);

    auto* qr = new QrCodeView(url);
    qr->setWidth(280);
    qr->setHeight(280);
    qr->setMarginBottom(12);
    holder->addView(qr);

    auto* urlLabel = new brls::Label();
    urlLabel->setText(url);
    urlLabel->setFontSize(18);
    urlLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    holder->addView(urlLabel);

    auto* dialog = new brls::Dialog(holder);
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
}

} // namespace artemis::ui

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
    const float quiet = side / static_cast<float>(size_ + 4);
    const float module = quiet;
    const float card = quiet * static_cast<float>(size_ + 4);
    const float originX = x + (width - card) * 0.5f + quiet * 2.0f;
    const float originY = y + (height - card) * 0.5f + quiet * 2.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + (width - card) * 0.5f, y + (height - card) * 0.5f,
                   card, card, 8.0f);
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
    holder->setPadding(24);

    auto* heading = new brls::Label();
    heading->setText(title.empty() ? "host/web_config"_i18n : title);
    heading->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    heading->setFontSize(22);
    heading->setMarginBottom(8);
    holder->addView(heading);

    auto* hint = new brls::Label();
    hint->setText("host/web_config_scan_hint"_i18n);
    hint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    hint->setFontSize(16);
    hint->setMarginBottom(16);
    holder->addView(hint);

    auto* qr = new QrCodeView(url);
    constexpr float kQrSize = 340.0f;
    qr->setWidth(kQrSize);
    qr->setHeight(kQrSize);
    qr->setMarginBottom(12);
    holder->addView(qr);

    if (!qr->valid()) {
        auto* fallback = new brls::Label();
        fallback->setText("host/web_config_qr_unavailable"_i18n);
        fallback->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        fallback->setFontSize(16);
        fallback->setMarginBottom(12);
        holder->addView(fallback);
    }

    auto* urlLabel = new brls::Label();
    urlLabel->setText(url);
    urlLabel->setFontSize(16);
    urlLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    urlLabel->setMarginBottom(4);
    holder->addView(urlLabel);

    auto* dialog = new brls::Dialog(holder);
#ifndef __SWITCH__
    auto* platform = brls::Application::getPlatform();
    if (platform) {
        dialog->addButton("host/web_config_copy"_i18n, [url, platform] {
            try {
                platform->pasteToClipboard(url);
            } catch (...) {
            }
        });
    }
#endif
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
}

} // namespace artemis::ui

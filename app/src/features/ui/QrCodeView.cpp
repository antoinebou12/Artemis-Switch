#include "QrCodeView.hpp"

#include "qrcodegen.hpp"

#include <algorithm>
#include <cmath>

using namespace brls::literals;

namespace artemis::ui {

QrCodeView::QrCodeView(const std::string& payload) {
    setHeight(360.0f);
    try {
        const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(
            payload.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        size_ = code.getSize();
        modules_.reserve(static_cast<std::size_t>(size_ * size_));
        for (int y = 0; y < size_; ++y) {
            for (int x = 0; x < size_; ++x)
                modules_.push_back(code.getModule(x, y) ? 1 : 0);
        }
    } catch (...) {
        modules_.clear();
        size_ = 0;
    }
}

void QrCodeView::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx) {
    (void)style;
    (void)ctx;
    if (!valid() || size_ <= 0)
        return;

    constexpr int quietZone = 4;
    const float available = std::max(1.0f, std::min(width, height));
    const float module =
        std::floor(available / static_cast<float>(size_ + quietZone * 2));
    if (module < 1.0f)
        return;
    const float imageSize =
        module * static_cast<float>(size_ + quietZone * 2);
    const float left = x + (width - imageSize) * 0.5f;
    const float top = y + (height - imageSize) * 0.5f;

    nvgBeginPath(vg);
    nvgRect(vg, left, top, imageSize, imageSize);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);

    nvgBeginPath(vg);
    for (int row = 0; row < size_; ++row) {
        for (int column = 0; column < size_; ++column) {
            if (!modules_[static_cast<std::size_t>(row * size_ + column)])
                continue;
            nvgRect(vg,
                    left + static_cast<float>(column + quietZone) * module,
                    top + static_cast<float>(row + quietZone) * module, module,
                    module);
        }
    }
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
}

void showUrlQrDialog(const std::string& title, const std::string& url) {
    auto* container = new brls::Box(brls::Axis::COLUMN);
    container->setAlignItems(brls::AlignItems::STRETCH);
    container->setWidth(500.0f);
    container->setPadding(18.0f);

    auto* heading = new brls::Label();
    heading->setText(title.empty() ? "host/web_config"_i18n : title);
    heading->setFontSize(20.0f);
    heading->setMarginBottom(4.0f);
    container->addView(heading);

    auto* hint = new brls::Label();
    hint->setText("host/web_config_scan_hint"_i18n);
    hint->setFontSize(16.0f);
    hint->setMarginBottom(8.0f);
    container->addView(hint);

    auto* qr = new QrCodeView(url);
    container->addView(qr);

    if (!qr->valid()) {
        auto* fallback = new brls::Label();
        fallback->setText("host/web_config_qr_unavailable"_i18n);
        fallback->setFontSize(16.0f);
        fallback->setMarginTop(8.0f);
        container->addView(fallback);
    }

    auto* urlLabel = new brls::Label();
    urlLabel->setText(url);
    urlLabel->setFontSize(16.0f);
    urlLabel->setMarginTop(8.0f);
    container->addView(urlLabel);

    auto* dialog = new brls::Dialog(container);
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

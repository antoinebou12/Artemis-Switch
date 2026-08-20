//
// ProfileEditorDialog.cpp — Settings-style full stream profile editor
//

#include "ProfileEditorDialog.hpp"

#include "FsrPreset.hpp"
#include "StreamConfigProfileNormalize.hpp"
#include "features/input/PointerSettings.hpp"
#include "features/input/SwitchMotionPolicy.hpp"
#include "features/stream/AdvancedStreamOptions.hpp"
#include "features/stream/FrameRateOptions.hpp"
#include "keyboard_view.hpp"
#include "views/boolean_slider_cell.hpp"

#include <algorithm>
#include <cmath>
#include <borealis.hpp>
#include <fmt/format.h>
#include <functional>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

static_assert(kFsrPresetUpscalingFsr1 == UPSCALING_FSR1);

void addHeader(brls::Box* content, const std::string& title,
               float paddingTop = 60.0f) {
    auto* header = new brls::Header();
    header->setTitle(title);
    // Keep section titles on one line (long locales / docked UI).
    if (auto* label = dynamic_cast<brls::Label*>(
            header->getView("brls/header/title")))
        label->setSingleLine(true);
    header->setMarginTop(paddingTop);
    content->addView(header);
}

brls::DetailCell* addDetail(brls::Box* content, const std::string& title,
                            const std::string& detail) {
    auto* cell = new brls::DetailCell();
    cell->setText(title);
    cell->setDetailText(detail);
    cell->title->setSingleLine(true);
    cell->detail->setSingleLine(true);
    content->addView(cell);
    return cell;
}

brls::BooleanCell* addBool(brls::Box* content, const std::string& title,
                           bool value,
                           const std::function<void(bool)>& onChange) {
    auto* cell = new brls::BooleanCell();
    cell->init(title, value, onChange);
    content->addView(cell);
    return cell;
}

float mouseSpeedScaleFromMultiplier(int multiplier) {
    // Match Settings::mouse_speed_scale(): slider 0..100 → 0.1x .. 2.0x.
    return 0.1f + (std::clamp(multiplier, 0, 100) / 100.0f) * 1.9f;
}

// Compact title + value + slider (no Header chrome). Nested Headers under a
// section look like a blank "newline" on docked 1080 UI.
brls::Label* addLabeledSlider(
    brls::Box* content, const std::string& title, const std::string& valueText,
    float progress, float marginTop,
    const std::function<void(float, brls::Label*)>& onProgress) {
    auto* block = new brls::Box(brls::Axis::COLUMN);
    block->setAlignItems(brls::AlignItems::STRETCH);
    block->setWidth(brls::View::AUTO);
    if (marginTop > 0.0f)
        block->setMarginTop(marginTop);

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setWidth(brls::View::AUTO);
    row->setHeight(brls::View::AUTO);
    row->setPaddingTop(8.0f);
    row->setPaddingBottom(4.0f);

    auto style = brls::Application::getStyle();
    auto theme = brls::Application::getTheme();

    auto* titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setSingleLine(true);
    titleLabel->setFontSize(style["brls/header/font_size"]);
    row->addView(titleLabel);
    row->addView(new brls::Padding());

    auto* valueLabel = new brls::Label();
    valueLabel->setText(valueText);
    valueLabel->setSingleLine(true);
    valueLabel->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    valueLabel->setFontSize(style["brls/header/font_size"]);
    valueLabel->setTextColor(theme["brls/header/subtitle"]);
    row->addView(valueLabel);
    block->addView(row);

    auto* slider = new brls::Slider();
    slider->setHeight(84.0f);
    slider->setWidth(brls::View::AUTO);
    slider->setGrow(1.0f);
    slider->setProgress(std::clamp(progress, 0.0f, 1.0f));
    slider->getProgressEvent()->subscribe(
        [onProgress, valueLabel](float p) { onProgress(p, valueLabel); });
    block->addView(slider);
    content->addView(block);
    return valueLabel;
}

void addBitrateSlider(brls::Box* content, StreamConfigProfile* draft) {
#if defined(__PSV__)
    const float mbpsMaxLimit = 20000.0f;
#elif defined(PLATFORM_SWITCH)
    const float mbpsMaxLimit = 100000.0f;
#else
    const float mbpsMaxLimit = 150000.0f;
#endif
    const float limitOffset = 500.0f;
    const float limit = mbpsMaxLimit - limitOffset;
    const float progress = std::clamp(
        (static_cast<float>(draft->bitrateKbps) - limitOffset) / limit, 0.0f,
        1.0f);
    addLabeledSlider(
        content, "settings/video_bitrate"_i18n,
        fmt::format("{:.1f} Mbps", draft->bitrateKbps / 1000.0), progress, 12.0f,
        [draft, limitOffset, limit](float p, brls::Label* valueLabel) {
            const int kbps = static_cast<int>(
                std::clamp(p, 0.0f, 1.0f) * limit + limitOffset);
            draft->bitrateKbps = kbps;
            valueLabel->setText(fmt::format("{:.1f} Mbps", kbps / 1000.0));
        });
}

void addRumbleSlider(brls::Box* content, StreamConfigProfile* draft) {
    addLabeledSlider(
        content, "settings/rumble_force"_i18n,
        fmt::format("{:.0f}%", draft->rumbleForce * 100.0f),
        std::clamp(draft->rumbleForce, 0.0f, 1.0f), 12.0f,
        [draft](float p, brls::Label* valueLabel) {
            draft->rumbleForce = std::clamp(p, 0.0f, 1.0f);
            valueLabel->setText(
                fmt::format("{:.0f}%", draft->rumbleForce * 100.0f));
        });
}

void addMouseSpeedSlider(brls::Box* content, StreamConfigProfile* draft) {
    // Nested under the Mouse section — keep a tight gap (docked UI exaggerates
    // Header chrome / lineBottom into a blank "newline").
    addLabeledSlider(
        content, "settings/mouse_speed"_i18n,
        fmt::format("{:.1f}x",
                    mouseSpeedScaleFromMultiplier(draft->mouseSpeedMultiplier)),
        std::clamp(draft->mouseSpeedMultiplier / 100.0f, 0.0f, 1.0f), 8.0f,
        [draft](float p, brls::Label* valueLabel) {
            draft->mouseSpeedMultiplier =
                std::clamp(static_cast<int>(p * 100.0f), 0, 100);
            valueLabel->setText(fmt::format(
                "{:.1f}x",
                mouseSpeedScaleFromMultiplier(draft->mouseSpeedMultiplier)));
        });
}

void addDitheringCell(brls::Box* content, StreamConfigProfile* draft) {
    auto* cell = new BooleanSliderCell();
    cell->init("settings/dithering"_i18n, draft->dithering,
               [draft, cell](bool on) {
                   draft->dithering = on;
                   cell->setSliderVisibility(
                       on ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
               });
    const float strength =
        std::clamp((draft->ditheringStrength - 1.0f) / 9.0f, 0.0f, 1.0f);
    cell->setProgress(strength);
    cell->setValueText(fmt::format("{:.1f}", draft->ditheringStrength));
    cell->setSliderVisibility(draft->dithering ? brls::Visibility::VISIBLE
                                               : brls::Visibility::GONE);
    cell->getProgressEvent()->subscribe([draft, cell](float p) {
        draft->ditheringStrength = 1.0f + std::clamp(p, 0.0f, 1.0f) * 9.0f;
        cell->setValueText(fmt::format("{:.1f}", draft->ditheringStrength));
    });
    content->addView(cell);
}

BooleanSliderCell* addRcasCell(brls::Box* content, StreamConfigProfile* draft,
                               const std::function<void()>& onUserEdit) {
    auto* cell = new BooleanSliderCell();
    cell->init("settings/rcas_sharpening"_i18n, draft->rcas,
               [draft, cell, onUserEdit](bool on) {
                   draft->rcas = on;
                   cell->setSliderVisibility(on ? brls::Visibility::VISIBLE
                                                : brls::Visibility::GONE);
                   if (onUserEdit)
                       onUserEdit();
               });
    cell->setProgress(std::clamp(draft->rcasStrength, 0.0f, 1.0f));
    cell->setValueText(
        fmt::format("{:.0f}%", draft->rcasStrength * 100.0f));
    cell->setSliderVisibility(draft->rcas ? brls::Visibility::VISIBLE
                                          : brls::Visibility::GONE);
    cell->getProgressEvent()->subscribe([draft, cell, onUserEdit](float p) {
        draft->rcasStrength = std::clamp(p, 0.0f, 1.0f);
        cell->setValueText(
            fmt::format("{:.0f}%", draft->rcasStrength * 100.0f));
        if (onUserEdit)
            onUserEdit();
    });
    content->addView(cell);
    return cell;
}

void syncRcasCell(BooleanSliderCell* cell, const StreamConfigProfile& draft) {
    cell->setOn(draft.rcas);
    cell->setProgress(std::clamp(draft.rcasStrength, 0.0f, 1.0f));
    cell->setValueText(
        fmt::format("{:.0f}%", draft.rcasStrength * 100.0f));
    cell->setSliderVisibility(draft.rcas ? brls::Visibility::VISIBLE
                                         : brls::Visibility::GONE);
}

std::string fsrPresetLabel(int preset) {
    switch (normalizeFsrPreset(preset)) {
    case FsrPreset::Performance:
        return "artemis/settings/fsr_preset_performance"_i18n;
    case FsrPreset::Balanced:
        return "artemis/settings/fsr_preset_balanced"_i18n;
    case FsrPreset::Quality:
        return "artemis/settings/fsr_preset_quality"_i18n;
    case FsrPreset::Off:
    default:
        return "hints/off"_i18n;
    }
}

void setFsrPresetRowEnabled(brls::DetailCell* cell, bool enabled) {
    cell->setFocusable(enabled);
    brls::Theme theme = brls::Application::getTheme();
    cell->title->setTextColor(enabled ? theme["brls/text"]
                                      : theme["brls/text_disabled"]);
    cell->setDetailTextColor(enabled
                                 ? theme["brls/list/listItem_value_color"]
                                 : theme["brls/text_disabled"]);
}

void maybeClearFsrPreset(StreamConfigProfile* draft, brls::DetailCell* cell) {
    if (draft->fsrPreset == 0)
        return;
    FsrPresetValues expected;
    if (!fsrPresetValues(normalizeFsrPreset(draft->fsrPreset), &expected))
        return;
    if (static_cast<int>(draft->upscalingMode) == expected.upscalingMode &&
        draft->rcas == expected.rcas &&
        std::abs(draft->rcasStrength - expected.rcasStrength) < 0.001f)
        return;
    draft->fsrPreset = 0;
    cell->setDetailText(fsrPresetLabel(0));
}

std::string heightLabel(int height) {
    if (height < 0)
        return "settings/resolution_native"_i18n;
    if (highResolutionMayLag(height))
        return fmt::format("{}p — {}", height,
                           "settings/may_lag"_i18n);
    return fmt::format("{}p", height);
}

std::string audioConfigLabel(StreamAudioConfiguration config) {
    return config == STREAM_AUDIO_51_SURROUND
               ? "settings/audio_51_surround"_i18n
               : "settings/audio_stereo"_i18n;
}

std::string aspectLabel(StreamAspectRatio aspect) {
    switch (normalizeAspectRatio(aspect)) {
    case StreamAspectRatio::Ratio4x3:
        return "settings/aspect_ratio_4_3"_i18n;
    case StreamAspectRatio::Ratio16x9:
    default:
        return "settings/aspect_ratio_16_9"_i18n;
    }
}

std::string codecLabel(VideoCodec codec) {
    switch (codec) {
    case H264:
        return "H.264";
    case AV1:
        return "AV1";
    case H265:
    default:
        return "H.265";
    }
}

std::string scaleLabel(artemis::video::ScaleMode mode) {
    switch (mode) {
    case artemis::video::ScaleMode::Fit:
        return "artemis/settings/fit"_i18n;
    case artemis::video::ScaleMode::Stretch:
        return "artemis/settings/stretch"_i18n;
    case artemis::video::ScaleMode::Fill:
    default:
        return "artemis/settings/fill"_i18n;
    }
}

std::string upscalingLabel(UpscalingMode mode) {
    switch (mode) {
    case UPSCALING_FSR1:
        return "FSR1";
    case UPSCALING_SGSR1:
        return "SGSR1";
    case UPSCALING_NIS:
        return "NIS";
    case UPSCALING_METALFX:
        return "MetalFX";
    case UPSCALING_OFF:
    default:
        return "hints/off"_i18n;
    }
}

std::string keyboardTypeLabel(KeyboardType type) {
    switch (type) {
    case FULLSIZED:
        return "settings/keyboard_fullsized"_i18n;
    case NUMPAD:
        return "settings/keyboard_numpad"_i18n;
    case COMPACT:
    default:
        return "settings/keyboard_compact"_i18n;
    }
}

std::string nativeScaleLabel(int scale) {
    switch (scale) {
    case 50:
        return "0.5x";
    case 75:
        return "0.75x";
    case 200:
        return "2.0x";
    case 100:
    default:
        return "1.0x";
    }
}

std::string pointerModeLabel(artemis::input::PointerMode mode) {
    using Mode = artemis::input::PointerMode;
    switch (mode) {
    case Mode::MultiTouch:
        return "artemis/overlay/touch_multitouch"_i18n;
    case Mode::Absolute:
        return "artemis/overlay/touch_absolute"_i18n;
    case Mode::AbsoluteSwapped:
        return "artemis/overlay/touch_absolute_swapped"_i18n;
    case Mode::TrackpadGaming:
        return "artemis/overlay/touch_trackpad_gaming"_i18n;
    case Mode::Disabled:
        return "artemis/overlay/touch_disabled"_i18n;
    case Mode::TrackpadNatural:
    default:
        return "artemis/overlay/touch_trackpad"_i18n;
    }
}

} // namespace

void openProfileEditor(const std::string& profileId,
                       const std::string& assignHostKey,
                       const std::function<void()>& onChanged) {
    auto* draft = new StreamConfigProfile();
    if (!profileId.empty()) {
        if (auto existing =
                StreamConfigProfileStore::instance().get(profileId)) {
            *draft = *existing;
        } else {
            *draft = StreamConfigProfileStore::snapshotFromSettings("Profile");
            draft->id = profileId;
        }
    } else {
        *draft = StreamConfigProfileStore::snapshotFromSettings("Profile");
        draft->id.clear();
        draft->name.clear();
    }

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    auto* content = new brls::Box(brls::Axis::COLUMN);
    // Match scroll viewport width (ScrollingFrame::onLayout). A fixed 10000px
    // width makes docked sliders lay out as an off-screen-wide track.
    content->setAlignItems(brls::AlignItems::STRETCH);
    content->setWidth(brls::View::AUTO);
    content->setPadding(
        brls::Application::getStyle()["brls/tab_details/padding_top"],
        brls::Application::getStyle()["brls/tab_details/padding_right"],
        brls::Application::getStyle()["brls/tab_details/padding_bottom"],
        brls::Application::getStyle()["brls/tab_details/padding_left"]);

    addHeader(content, "artemis/settings/profiles"_i18n, 0.0f);
    auto* nameCell =
        addDetail(content, "artemis/settings/profile_name"_i18n,
                  draft->name.empty() ? "artemis/settings/profile_name_hint"_i18n
                                      : draft->name);
    nameCell->registerClickAction([draft, nameCell](brls::View*) {
        brls::Application::getPlatform()->getImeManager()->openForText(
            [draft, nameCell](const std::string& text) {
                if (text.empty())
                    return;
                draft->name = text;
                nameCell->setDetailText(text);
            },
            "artemis/settings/profile_name"_i18n, "", 40, draft->name, 0);
        return true;
    });
    addBool(content, "artemis/settings/hide_profile"_i18n, draft->hidden,
            [draft](bool v) { draft->hidden = v; });

    addHeader(content, "artemis/settings/profile_section_video"_i18n);

    auto* fpsCell = addDetail(
        content, "settings/fps"_i18n,
        artemis::stream::frameRatePresetLabel(
            artemis::stream::availableFrameRatePresets()[static_cast<size_t>(
                artemis::stream::frameRatePresetIndex(
                    draft->fps, draft->clientRefreshRateX100))]));
    fpsCell->registerClickAction([draft, fpsCell](brls::View*) {
        const auto presets = artemis::stream::availableFrameRatePresets();
        std::vector<std::string> labels;
        labels.reserve(presets.size());
        for (const auto& preset : presets)
            labels.push_back(artemis::stream::frameRatePresetLabel(preset));
        const int selected = artemis::stream::frameRatePresetIndex(
            draft->fps, draft->clientRefreshRateX100);
        auto* dropdown = new brls::Dropdown(
            "settings/fps"_i18n, labels,
            [draft, fpsCell, presets](int index) {
                if (index < 0 || index >= static_cast<int>(presets.size()))
                    return;
                const auto& preset = presets[static_cast<size_t>(index)];
                draft->fps = preset.fps;
                draft->clientRefreshRateX100 = preset.clientRefreshRateX100;
                fpsCell->setDetailText(
                    artemis::stream::frameRatePresetLabel(preset));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto* resCell =
        addDetail(content, "settings/resolution"_i18n,
                  heightLabel(draft->resolutionHeight));

    auto* aspectCell =
        addDetail(content, "settings/aspect_ratio"_i18n,
                  aspectLabel(draft->aspectRatio));
    aspectCell->registerClickAction([draft, aspectCell](brls::View*) {
        const std::vector<StreamAspectRatio> values = {
            StreamAspectRatio::Ratio16x9, StreamAspectRatio::Ratio4x3};
        std::vector<std::string> options;
        int selected = 0;
        for (size_t i = 0; i < values.size(); ++i) {
            options.push_back(aspectLabel(values[i]));
            if (values[i] == draft->aspectRatio)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/aspect_ratio"_i18n, options,
            [draft, aspectCell, values](int index) {
                if (index >= 0 && index < static_cast<int>(values.size()))
                    draft->aspectRatio = values[static_cast<size_t>(index)];
                aspectCell->setDetailText(aspectLabel(draft->aspectRatio));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto* customToggle = new brls::BooleanCell();
    content->addView(customToggle);
    auto* customW = addDetail(content, "artemis/settings/custom_width"_i18n,
                              std::to_string(draft->customWidth));
    customW->registerClickAction([draft, customW](brls::View*) {
        brls::Application::getImeManager()->openForNumber(
            [draft, customW](long number) {
                draft->customWidth =
                    normalizeCustomDimension(static_cast<int>(number), 1920);
                customW->setDetailText(std::to_string(draft->customWidth));
            },
            "artemis/settings/custom_width"_i18n, "", 5,
            std::to_string(draft->customWidth), "", "", 0);
        return true;
    });
    auto* customH = addDetail(content, "artemis/settings/custom_height"_i18n,
                              std::to_string(draft->customHeight));
    customH->registerClickAction([draft, customH](brls::View*) {
        brls::Application::getImeManager()->openForNumber(
            [draft, customH](long number) {
                draft->customHeight =
                    normalizeCustomDimension(static_cast<int>(number), 1080);
                customH->setDetailText(std::to_string(draft->customHeight));
            },
            "artemis/settings/custom_height"_i18n, "", 5,
            std::to_string(draft->customHeight), "", "", 0);
        return true;
    });
    const auto customResVisibility = draft->customResolutionEnabled
                                         ? brls::Visibility::VISIBLE
                                         : brls::Visibility::GONE;
    customW->setVisibility(customResVisibility);
    customH->setVisibility(customResVisibility);
    customToggle->init(
        "artemis/settings/use_custom_resolution"_i18n,
        draft->customResolutionEnabled,
        [draft, customW, customH](bool enabled) {
            draft->customResolutionEnabled = enabled;
            const auto visibility = enabled ? brls::Visibility::VISIBLE
                                            : brls::Visibility::GONE;
            customW->setVisibility(visibility);
            customH->setVisibility(visibility);
        });

    auto* nativeScaleCell =
        addDetail(content, "settings/resolution_scale"_i18n,
                  nativeScaleLabel(draft->nativeResolutionScale));
    nativeScaleCell->registerClickAction([draft, nativeScaleCell](brls::View*) {
        const std::vector<int> values = {50, 75, 100, 200};
        std::vector<std::string> labels;
        int selected = 2;
        for (size_t i = 0; i < values.size(); ++i) {
            labels.push_back(nativeScaleLabel(values[i]));
            if (values[i] == draft->nativeResolutionScale)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/resolution_scale"_i18n, labels,
            [draft, nativeScaleCell, values](int index) {
                if (index >= 0 && index < static_cast<int>(values.size()))
                    draft->nativeResolutionScale =
                        values[static_cast<size_t>(index)];
                nativeScaleCell->setDetailText(
                    nativeScaleLabel(draft->nativeResolutionScale));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    auto updateNativeScaleVisibility = [draft, nativeScaleCell]() {
        nativeScaleCell->setVisibility(draft->resolutionHeight < 0
                                           ? brls::Visibility::VISIBLE
                                           : brls::Visibility::GONE);
    };
    updateNativeScaleVisibility();
    resCell->registerClickAction(
        [draft, resCell, updateNativeScaleVisibility](brls::View*) {
            const std::vector<int> heights = {-1, 360, 480, 540, 720, 1080,
                                              1440};
            std::vector<std::string> options;
            int selected = 4;
            for (size_t i = 0; i < heights.size(); ++i) {
                options.push_back(heightLabel(heights[i]));
                if (heights[i] == draft->resolutionHeight)
                    selected = static_cast<int>(i);
            }
            auto* dropdown = new brls::Dropdown(
                "settings/resolution"_i18n, options,
                [draft, resCell, heights,
                 updateNativeScaleVisibility](int index) {
                    if (index >= 0 && index < static_cast<int>(heights.size()))
                        draft->resolutionHeight =
                            heights[static_cast<size_t>(index)];
                    resCell->setDetailText(
                        heightLabel(draft->resolutionHeight));
                    updateNativeScaleVisibility();
                },
                selected);
            brls::Application::pushActivity(new brls::Activity(dropdown));
            return true;
        });

    auto* hwCell =
        addBool(content, "settings/use_hw_decoding"_i18n, draft->useHwDecoding,
                [draft](bool v) { draft->useHwDecoding = v; });
#if defined(__linux__) && defined(PLATFORM_DESKTOP)
    hwCell->setEnabled(true);
#else
    hwCell->setEnabled(false);
#endif

    auto* codecCell =
        addDetail(content, "settings/video_codec"_i18n, codecLabel(draft->videoCodec));
    codecCell->registerClickAction([draft, codecCell](brls::View*) {
        const std::vector<std::string> options = {"H.264", "H.265", "AV1"};
        int selected = draft->videoCodec == H264   ? 0
                       : draft->videoCodec == AV1  ? 2
                                                   : 1;
        auto* dropdown = new brls::Dropdown(
            "settings/video_codec"_i18n, options,
            [draft, codecCell](int index) {
                draft->videoCodec =
                    index == 0 ? H264 : (index == 2 ? AV1 : H265);
                codecCell->setDetailText(codecLabel(draft->videoCodec));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto* hdrCell =
        addBool(content, "settings/request_hdr"_i18n, draft->requestHdr,
                [draft](bool v) { draft->requestHdr = v; });
#ifndef SUPPORT_HDR
    hdrCell->setVisibility(brls::Visibility::GONE);
#else
    (void)hdrCell;
#endif

    auto* decoderCell = addDetail(
        content, "settings/decoder_threads"_i18n,
        draft->decoderThreads == 0 ? "settings/zero_threads"_i18n
                                   : std::to_string(draft->decoderThreads));
    decoderCell->registerClickAction([draft, decoderCell](brls::View*) {
        const std::vector<int> values = {0, 2, 3, 4};
        std::vector<std::string> labels;
        int selected = 3;
        for (size_t i = 0; i < values.size(); ++i) {
            labels.push_back(values[i] == 0 ? "settings/zero_threads"_i18n
                                            : std::to_string(values[i]));
            if (values[i] == draft->decoderThreads)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/decoder_threads"_i18n, labels,
            [draft, decoderCell, values](int index) {
                if (index >= 0 && index < static_cast<int>(values.size()))
                    draft->decoderThreads = values[static_cast<size_t>(index)];
                decoderCell->setDetailText(
                    draft->decoderThreads == 0 ? "settings/zero_threads"_i18n
                                               : std::to_string(draft->decoderThreads));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    addBitrateSlider(content, draft);

    addHeader(content, "artemis/settings/profile_section_presentation"_i18n);
    addDitheringCell(content, draft);
    auto* upscaleCell =
        addDetail(content, "settings/upscaling"_i18n, upscalingLabel(draft->upscalingMode));
    auto* fsrPresetCell =
        addDetail(content, "artemis/settings/fsr_preset"_i18n,
                  fsrPresetLabel(draft->fsrPreset));
    auto updateFsrPresetEnabled = [draft, fsrPresetCell]() {
        setFsrPresetRowEnabled(
            fsrPresetCell,
            fsrPresetSelectable(static_cast<int>(draft->upscalingMode)));
    };
    updateFsrPresetEnabled();
    auto* rcasCell = addRcasCell(content, draft, [draft, fsrPresetCell]() {
        maybeClearFsrPreset(draft, fsrPresetCell);
    });
    upscaleCell->registerClickAction(
        [draft, upscaleCell, fsrPresetCell, updateFsrPresetEnabled](brls::View*) {
        const std::vector<std::string> options = {
            "hints/off"_i18n, "FSR1", "SGSR1", "NIS"};
        int selected = 0;
        switch (draft->upscalingMode) {
        case UPSCALING_FSR1:
            selected = 1;
            break;
        case UPSCALING_SGSR1:
            selected = 2;
            break;
        case UPSCALING_NIS:
            selected = 3;
            break;
        default:
            selected = 0;
            break;
        }
        auto* dropdown = new brls::Dropdown(
            "settings/upscaling"_i18n, options,
            [draft, upscaleCell, fsrPresetCell,
             updateFsrPresetEnabled](int index) {
                draft->upscalingMode =
                    index == 1   ? UPSCALING_FSR1
                    : index == 2 ? UPSCALING_SGSR1
                    : index == 3 ? UPSCALING_NIS
                                 : UPSCALING_OFF;
                if (draft->upscalingMode == UPSCALING_OFF)
                    draft->fsrPreset = 0;
                upscaleCell->setDetailText(upscalingLabel(draft->upscalingMode));
                fsrPresetCell->setDetailText(fsrPresetLabel(draft->fsrPreset));
                updateFsrPresetEnabled();
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    fsrPresetCell->registerClickAction(
        [draft, upscaleCell, fsrPresetCell, rcasCell,
         updateFsrPresetEnabled](brls::View*) {
        if (!fsrPresetSelectable(static_cast<int>(draft->upscalingMode)))
            return false;
        const std::vector<FsrPreset> values = {
            FsrPreset::Off, FsrPreset::Performance, FsrPreset::Balanced,
            FsrPreset::Quality};
        std::vector<std::string> options;
        int selected = 0;
        const auto current = normalizeFsrPreset(draft->fsrPreset);
        for (size_t i = 0; i < values.size(); ++i) {
            options.push_back(fsrPresetLabel(static_cast<int>(values[i])));
            if (values[i] == current)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "artemis/settings/fsr_preset"_i18n, options,
            [draft, upscaleCell, fsrPresetCell, rcasCell,
             updateFsrPresetEnabled, values](int index) {
                if (index < 0 || index >= static_cast<int>(values.size()))
                    return;
                draft->fsrPreset = static_cast<int>(values[static_cast<size_t>(index)]);
                int mode = static_cast<int>(draft->upscalingMode);
                bool rcas = draft->rcas;
                float strength = draft->rcasStrength;
                if (applyFsrPreset(draft->fsrPreset, &mode, &rcas, &strength)) {
                    draft->upscalingMode = static_cast<UpscalingMode>(mode);
                    draft->rcas = rcas;
                    draft->rcasStrength = strength;
                    upscaleCell->setDetailText(
                        upscalingLabel(draft->upscalingMode));
                    syncRcasCell(rcasCell, *draft);
                }
                fsrPresetCell->setDetailText(fsrPresetLabel(draft->fsrPreset));
                updateFsrPresetEnabled();
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto* scaleCell = addDetail(content, "artemis/settings/video_scale_mode"_i18n,
                                scaleLabel(draft->scaleMode));
    scaleCell->registerClickAction([draft, scaleCell](brls::View*) {
        const std::vector<std::string> options = {
            "artemis/settings/fit"_i18n, "artemis/settings/fill"_i18n,
            "artemis/settings/stretch"_i18n};
        int selected = draft->scaleMode == artemis::video::ScaleMode::Fit   ? 0
                       : draft->scaleMode == artemis::video::ScaleMode::Stretch
                           ? 2
                           : 1;
        auto* dropdown = new brls::Dropdown(
            "artemis/settings/video_scale_mode"_i18n, options,
            [draft, scaleCell](int index) {
                draft->scaleMode =
                    index == 0   ? artemis::video::ScaleMode::Fit
                    : index == 2 ? artemis::video::ScaleMode::Stretch
                                 : artemis::video::ScaleMode::Fill;
                scaleCell->setDetailText(scaleLabel(draft->scaleMode));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    addBool(content, "artemis/settings/remember_zoom_pan"_i18n,
            draft->rememberZoomPan,
            [draft](bool v) { draft->rememberZoomPan = v; });

    addHeader(content, "artemis/settings/switch_motion"_i18n);
    addBool(content, "artemis/settings/forward_motion"_i18n, draft->forwardMotion,
            [draft](bool v) { draft->forwardMotion = v; });
    {
        const auto capabilities =
            artemis::input::detectSwitchMotionCapabilities();
        const bool consoleFallbackSupported =
            artemis::input::canEnableConsoleMotionFallback(capabilities);
        if (!consoleFallbackSupported)
            draft->consoleMotionFallback = false;
        auto* consoleCell = addBool(
            content, "artemis/settings/console_motion_fallback"_i18n,
            consoleFallbackSupported && draft->consoleMotionFallback,
            [draft, consoleFallbackSupported](bool v) {
                if (!consoleFallbackSupported)
                    return;
                draft->consoleMotionFallback = v;
            });
        consoleCell->setEnabled(consoleFallbackSupported);
    }

    addHeader(content, "artemis/settings/profile_section_audio"_i18n);
    addBool(content, "settings/paop"_i18n, draft->playAudioOnPc,
            [draft](bool v) { draft->playAudioOnPc = v; });
    auto* audioCell =
        addDetail(content, "settings/stream_audio_configuration"_i18n,
                  audioConfigLabel(draft->streamAudioConfiguration));
    audioCell->registerClickAction([draft, audioCell](brls::View*) {
        const std::vector<std::string> options = {
            "settings/audio_stereo"_i18n, "settings/audio_51_surround"_i18n};
        auto* dropdown = new brls::Dropdown(
            "settings/stream_audio_configuration"_i18n, options,
            [draft, audioCell](int index) {
                draft->streamAudioConfiguration =
                    index == 1 ? STREAM_AUDIO_51_SURROUND
                               : STREAM_AUDIO_STEREO;
                audioCell->setDetailText(
                    audioConfigLabel(draft->streamAudioConfiguration));
            },
            draft->streamAudioConfiguration == STREAM_AUDIO_51_SURROUND ? 1
                                                                        : 0);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
#ifdef __SWITCH__
    draft->audioBackend = AUDREN;
    addDetail(content, "settings/audio_backend"_i18n, "Audren")
        ->setFocusable(false);
#else
    draft->audioBackend = SDL;
    addDetail(content, "settings/audio_backend"_i18n, "SDL")->setFocusable(false);
#endif
    addBool(content, "settings/volume_amplification"_i18n,
            draft->volumeAmplification,
            [draft](bool v) { draft->volumeAmplification = v; });
    addLabeledSlider(
        content, "streaming/volume"_i18n,
        fmt::format("{}%", std::clamp(draft->volume, 0, 100)),
        std::clamp(draft->volume, 0, 100) / 100.0f, 12.0f,
        [draft](float p, brls::Label* valueLabel) {
            const int volume =
                static_cast<int>(std::clamp(p, 0.0f, 1.0f) * 100.0f);
            draft->volume = volume;
            valueLabel->setText(fmt::format("{}%", volume));
        });

    addHeader(content, "artemis/settings/profile_section_stream"_i18n);
    addBool(content, "settings/usops"_i18n, draft->sops,
            [draft](bool v) { draft->sops = v; });
    addBool(content, "settings/terminate_app_on_disconnect"_i18n,
            draft->terminateAppOnDisconnect,
            [draft](bool v) { draft->terminateAppOnDisconnect = v; });
    addBool(content, "artemis/settings/force_full_range"_i18n,
            draft->forceFullRangeVideo,
            [draft](bool v) { draft->forceFullRangeVideo = v; });
    addBool(content, "artemis/settings/prevent_packet_loss"_i18n,
            draft->preventPacketLoss,
            [draft](bool v) { draft->preventPacketLoss = v; });
    addBool(content, "artemis/settings/low_latency_pacing"_i18n,
            draft->lowLatencyPacing,
            [draft](bool v) { draft->lowLatencyPacing = v; });
    {
        auto* queueCell =
            addDetail(content, "artemis/settings/frames_queue_size"_i18n,
                      std::to_string(std::clamp(draft->framesQueueSize, 1, 5)));
        queueCell->registerClickAction([draft, queueCell](brls::View*) {
            std::vector<std::string> labels = {"1", "2", "3", "4", "5"};
            int selected = std::clamp(draft->framesQueueSize, 1, 5) - 1;
            auto* dropdown = new brls::Dropdown(
                "artemis/settings/frames_queue_size"_i18n, labels,
                [draft, queueCell](int index) {
                    draft->framesQueueSize = std::clamp(index + 1, 1, 5);
                    queueCell->setDetailText(
                        std::to_string(draft->framesQueueSize));
                },
                selected);
            brls::Application::pushActivity(new brls::Activity(dropdown));
            return true;
        });
    }

    auto packetSizeLabel = [](int size) -> std::string {
        if (size <= 0)
            return "artemis/settings/packet_size_auto"_i18n;
        return std::to_string(size);
    };
    auto* packetSizeCell =
        addDetail(content, "artemis/settings/packet_size"_i18n,
                  packetSizeLabel(draft->packetSize));
    packetSizeCell->registerClickAction(
        [draft, packetSizeCell, packetSizeLabel](brls::View*) {
            const std::vector<int> presets = {
                artemis::stream::kPacketSizeAuto, 1024, 1346,
                artemis::stream::kPacketSizeDefault};
            std::vector<std::string> labels = {
                "artemis/settings/packet_size_auto"_i18n, "1024", "1346",
                "1392", "artemis/settings/packet_size_custom"_i18n};
            int selected = 4;
            for (size_t i = 0; i < presets.size(); ++i) {
                if (presets[i] == draft->packetSize) {
                    selected = static_cast<int>(i);
                    break;
                }
            }
            auto* dropdown = new brls::Dropdown(
                "artemis/settings/packet_size"_i18n, labels,
                [draft, packetSizeCell, packetSizeLabel,
                 presets](int index) {
                    if (index >= 0 &&
                        index < static_cast<int>(presets.size())) {
                        draft->packetSize = presets[static_cast<size_t>(index)];
                        packetSizeCell->setDetailText(
                            packetSizeLabel(draft->packetSize));
                        return;
                    }
                    const int current =
                        draft->packetSize > 0
                            ? draft->packetSize
                            : artemis::stream::kPacketSizeDefault;
                    brls::Application::getImeManager()->openForNumber(
                        [draft, packetSizeCell,
                         packetSizeLabel](long number) {
                            draft->packetSize =
                                artemis::stream::clampPacketSize(
                                    static_cast<int>(number));
                            packetSizeCell->setDetailText(
                                packetSizeLabel(draft->packetSize));
                        },
                        "artemis/settings/packet_size"_i18n,
                        "artemis/settings/packet_size_hint"_i18n, 5,
                        std::to_string(current), "", "", 0);
                },
                selected);
            brls::Application::pushActivity(new brls::Activity(dropdown));
            return true;
        });

    addHeader(content, "artemis/settings/profile_section_controller"_i18n);
    {
        auto* layouts = Settings::instance().get_mapping_laouts();
        std::vector<std::string> layoutTitles;
        int selected = 0;
        if (layouts) {
            layoutTitles.reserve(layouts->size());
            for (size_t i = 0; i < layouts->size(); ++i) {
                layoutTitles.push_back((*layouts)[i].title);
                if (!draft->mappingLayoutTitle.empty() &&
                    (*layouts)[i].title == draft->mappingLayoutTitle)
                    selected = static_cast<int>(i);
            }
            if (draft->mappingLayoutTitle.empty() && !layoutTitles.empty()) {
                const int current = Settings::instance().get_current_mapping_layout();
                if (current >= 0 &&
                    current < static_cast<int>(layoutTitles.size()))
                    selected = current;
                draft->mappingLayoutTitle =
                    layoutTitles[static_cast<size_t>(selected)];
            }
        }
        const std::string detail = draft->mappingLayoutTitle.empty()
                                       ? "—"
                                       : draft->mappingLayoutTitle;
        auto* layoutCell =
            addDetail(content, "settings/keys_mapping_title"_i18n, detail);
        if (!layoutTitles.empty()) {
            layoutCell->registerClickAction(
                [draft, layoutCell, layoutTitles](brls::View*) {
                    int selected = 0;
                    for (size_t i = 0; i < layoutTitles.size(); ++i) {
                        if (layoutTitles[i] == draft->mappingLayoutTitle)
                            selected = static_cast<int>(i);
                    }
                    auto* dropdown = new brls::Dropdown(
                        "settings/keys_mapping_title"_i18n, layoutTitles,
                        [draft, layoutCell, layoutTitles](int index) {
                            if (index >= 0 &&
                                index < static_cast<int>(layoutTitles.size())) {
                                draft->mappingLayoutTitle =
                                    layoutTitles[static_cast<size_t>(index)];
                                layoutCell->setDetailText(
                                    draft->mappingLayoutTitle);
                            }
                        },
                        selected);
                    brls::Application::pushActivity(
                        new brls::Activity(dropdown));
                    return true;
                });
        } else {
            layoutCell->setFocusable(false);
        }
    }
    addBool(content, "settings/swap_ui_ab"_i18n, draft->swapUiAb,
            [draft](bool v) { draft->swapUiAb = v; });
    addBool(content, "settings/swap_ui_xy"_i18n, draft->swapUiXy,
            [draft](bool v) { draft->swapUiXy = v; });

    auto* dzL = addDetail(
        content, "settings/deadzone/stick_left"_i18n,
        fmt::format("{:.0f}%", draft->deadzoneLeft * 100.0f));
    dzL->registerClickAction([draft, dzL](brls::View*) {
        brls::Application::getImeManager()->openForNumber(
            [draft, dzL](long number) {
                draft->deadzoneLeft =
                    std::clamp(static_cast<float>(number), 0.0f, 100.0f) /
                    100.0f;
                dzL->setDetailText(
                    fmt::format("{:.0f}%", draft->deadzoneLeft * 100.0f));
            },
            "settings/deadzone/stick_left"_i18n, "", 3,
            std::to_string(static_cast<int>(draft->deadzoneLeft * 100)), "", "",
            0);
        return true;
    });
    auto* dzR = addDetail(
        content, "settings/deadzone/stick_right"_i18n,
        fmt::format("{:.0f}%", draft->deadzoneRight * 100.0f));
    dzR->registerClickAction([draft, dzR](brls::View*) {
        brls::Application::getImeManager()->openForNumber(
            [draft, dzR](long number) {
                draft->deadzoneRight =
                    std::clamp(static_cast<float>(number), 0.0f, 100.0f) /
                    100.0f;
                dzR->setDetailText(
                    fmt::format("{:.0f}%", draft->deadzoneRight * 100.0f));
            },
            "settings/deadzone/stick_right"_i18n, "", 3,
            std::to_string(static_cast<int>(draft->deadzoneRight * 100)), "",
            "", 0);
        return true;
    });

    addRumbleSlider(content, draft);
    addBool(content, "settings/swap_stick_to_dpad"_i18n, draft->swapStickToDpad,
            [draft](bool v) { draft->swapStickToDpad = v; });

    addHeader(content, "artemis/settings/profile_section_keyboard"_i18n);
    auto* kbType = addDetail(content, "settings/keyboard_type"_i18n,
                             keyboardTypeLabel(draft->keyboardType));
    kbType->registerClickAction([draft, kbType](brls::View*) {
        const std::vector<std::string> options = {
            "settings/keyboard_compact"_i18n, "settings/keyboard_fullsized"_i18n,
            "settings/keyboard_numpad"_i18n};
        int selected = draft->keyboardType == FULLSIZED ? 1
                       : draft->keyboardType == NUMPAD  ? 2
                                                        : 0;
        auto* dropdown = new brls::Dropdown(
            "settings/keyboard_type"_i18n, options,
            [draft, kbType](int index) {
                draft->keyboardType =
                    index == 1 ? FULLSIZED : (index == 2 ? NUMPAD : COMPACT);
                kbType->setDetailText(keyboardTypeLabel(draft->keyboardType));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    KeyboardView::ensureLocales();
    const auto locales = KeyboardView::getLocales();
    std::vector<std::string> localeNames;
    localeNames.reserve(locales.size());
    for (const auto& locale : locales)
        localeNames.push_back(locale.name);
    std::string localeDetail =
        draft->keyboardLocale >= 0 &&
                draft->keyboardLocale < static_cast<int>(localeNames.size())
            ? localeNames[static_cast<size_t>(draft->keyboardLocale)]
            : localeNames.empty() ? "" : localeNames.front();
    auto* kbLocale =
        addDetail(content, "settings/keyboard_layout"_i18n, localeDetail);
    kbLocale->registerClickAction([draft, kbLocale, localeNames](brls::View*) {
        int selected = std::clamp(draft->keyboardLocale, 0,
                                  static_cast<int>(localeNames.size()) - 1);
        auto* dropdown = new brls::Dropdown(
            "settings/keyboard_layout"_i18n, localeNames,
            [draft, kbLocale, localeNames](int index) {
                if (index >= 0 && index < static_cast<int>(localeNames.size())) {
                    draft->keyboardLocale = index;
                    kbLocale->setDetailText(
                        localeNames[static_cast<size_t>(index)]);
                }
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto fingersLabel = [](int fingers) -> std::string {
        if (fingers < 0)
            return "hints/off"_i18n;
        return std::to_string(fingers);
    };
    auto* kbFingers = addDetail(content, "settings/keyboard_fingers"_i18n,
                                fingersLabel(draft->keyboardFingers));
    kbFingers->registerClickAction([draft, kbFingers, fingersLabel](brls::View*) {
        const std::vector<int> values = {3, 4, 5, -1};
        std::vector<std::string> labels;
        int selected = 0;
        for (size_t i = 0; i < values.size(); ++i) {
            labels.push_back(fingersLabel(values[i]));
            if (values[i] == draft->keyboardFingers)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/keyboard_fingers"_i18n, labels,
            [draft, kbFingers, values, fingersLabel](int index) {
                if (index >= 0 && index < static_cast<int>(values.size()))
                    draft->keyboardFingers = values[static_cast<size_t>(index)];
                kbFingers->setDetailText(fingersLabel(draft->keyboardFingers));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    addHeader(content, "artemis/settings/profile_section_mouse"_i18n);
    auto* pointerCell = new brls::DetailCell();
    pointerCell->setText("artemis/overlay/pointer_mode"_i18n);
    pointerCell->setDetailText(pointerModeLabel(draft->pointerMode));
    pointerCell->title->setSingleLine(true);
    pointerCell->detail->setSingleLine(true);
    auto* touchscreenCell = addBool(
        content, "settings/touchscreen_mouse_mode"_i18n,
        draft->touchscreenMouseMode, [draft, pointerCell](bool value) {
            draft->touchscreenMouseMode = value;
            draft->pointerMode =
                artemis::input::pointerModeFromLegacyTouchscreen(value);
            pointerCell->setDetailText(pointerModeLabel(draft->pointerMode));
        });
    content->addView(pointerCell);
    pointerCell->registerClickAction(
        [draft, pointerCell, touchscreenCell](brls::View*) {
            using Mode = artemis::input::PointerMode;
            const std::vector<Mode> modes = {
                Mode::TrackpadNatural, Mode::TrackpadGaming, Mode::MultiTouch,
                Mode::Absolute,        Mode::AbsoluteSwapped, Mode::Disabled};
            std::vector<std::string> labels;
            int selected = 0;
            for (size_t i = 0; i < modes.size(); ++i) {
                labels.push_back(pointerModeLabel(modes[i]));
                if (modes[i] == draft->pointerMode)
                    selected = static_cast<int>(i);
            }
            auto* dropdown = new brls::Dropdown(
                "artemis/overlay/pointer_mode"_i18n, labels,
                [draft, pointerCell, touchscreenCell, modes](int index) {
                    if (index >= 0 &&
                        index < static_cast<int>(modes.size())) {
                        draft->pointerMode = modes[static_cast<size_t>(index)];
                        draft->touchscreenMouseMode =
                            draft->pointerMode == Mode::MultiTouch;
                        touchscreenCell->setOn(draft->touchscreenMouseMode,
                                               false);
                    }
                    pointerCell->setDetailText(
                        pointerModeLabel(draft->pointerMode));
                },
                selected);
            brls::Application::pushActivity(new brls::Activity(dropdown));
            return true;
        });
    addBool(content, "settings/swap_mouse_keys"_i18n, draft->swapMouseKeys,
            [draft](bool v) { draft->swapMouseKeys = v; });
    addBool(content, "settings/swap_mouse_scroll"_i18n, draft->swapMouseScroll,
            [draft](bool v) { draft->swapMouseScroll = v; });
    addBool(content, "settings/swap_mouse_sticks"_i18n, draft->swapMouseSticks,
            [draft](bool v) { draft->swapMouseSticks = v; });
    addMouseSpeedSlider(content, draft);

    auto persistDraft = [draft, profileId, assignHostKey, onChanged]() {
        if (draft->name.empty())
            draft->name = "Profile";
        auto& store = StreamConfigProfileStore::instance();
        std::string id = profileId;
        if (id.empty()) {
            auto created = store.create(draft->name, false);
            draft->id = created.id;
            id = created.id;
        }
        draft->id = id;
        store.update(*draft);
        if (!assignHostKey.empty())
            store.setSelectedForHost(assignHostKey, id);
        delete draft;
        // Pop first so list refresh callbacks see the correct stack.
        brls::Application::popActivity(brls::TransitionAnimation::FADE,
                                       [onChanged] {
                                           if (onChanged)
                                               onChanged();
                                       });
    };

    auto* saveCell = addDetail(content, "common/confirm"_i18n, "");
    saveCell->registerClickAction([persistDraft](brls::View*) {
        persistDraft();
        return true;
    });
    auto* cancelCell = addDetail(content, "common/cancel"_i18n, "");
    cancelCell->registerClickAction([draft](brls::View*) {
        delete draft;
        brls::Application::popActivity();
        return true;
    });

    scroll->setContentView(content);

    auto* frame = new brls::AppletFrame(scroll);
    frame->setTitle(profileId.empty() ? "artemis/settings/create_profile"_i18n
                                      : "host/edit_profile"_i18n);
    // B / Back auto-saves; Cancel row still discards.
    frame->registerAction("common/confirm"_i18n, brls::BUTTON_B,
                          [persistDraft](brls::View*) {
                              persistDraft();
                              return true;
                          });
    brls::Application::pushActivity(new brls::Activity(frame));
}

} // namespace artemis::streaming

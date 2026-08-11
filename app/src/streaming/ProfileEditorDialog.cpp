//
// ProfileEditorDialog.cpp — Settings-style full stream profile editor
//

#include "ProfileEditorDialog.hpp"

#include "keyboard_view.hpp"
#include "views/boolean_slider_cell.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <fmt/format.h>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

void addHeader(brls::Box* content, const std::string& title,
               float paddingTop = 60.0f) {
    auto* header = new brls::Header();
    header->setTitle(title);
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

void addBitrateSlider(brls::Box* content, StreamConfigProfile* draft) {
    auto* header = new brls::Header();
    header->setTitle("settings/video_bitrate"_i18n);
    header->setSubtitle(
        fmt::format("{:.1f} Mbps", draft->bitrateKbps / 1000.0));
    header->setMarginTop(60.0f);
    content->addView(header);

    auto* slider = new brls::Slider();
    slider->setHeight(84.0f);
    // Settings maps ~0–1 to bitrate range; mirror common 0.5–50 Mbps feel.
    const float progress =
        std::clamp(draft->bitrateKbps / 50000.0f, 0.02f, 1.0f);
    slider->setProgress(progress);
    slider->getProgressEvent()->subscribe([draft, header](float p) {
        const int kbps =
            std::clamp(static_cast<int>(p * 50000.0f), 1000, 100000);
        draft->bitrateKbps = kbps;
        header->setSubtitle(fmt::format("{:.1f} Mbps", kbps / 1000.0));
    });
    content->addView(slider);
}

void addRumbleSlider(brls::Box* content, StreamConfigProfile* draft) {
    auto* header = new brls::Header();
    header->setTitle("settings/rumble_force"_i18n);
    header->setSubtitle(fmt::format("{:.0f}%", draft->rumbleForce * 100.0f));
    header->setMarginTop(60.0f);
    content->addView(header);

    auto* slider = new brls::Slider();
    slider->setHeight(84.0f);
    slider->setProgress(std::clamp(draft->rumbleForce, 0.0f, 1.0f));
    slider->getProgressEvent()->subscribe([draft, header](float p) {
        draft->rumbleForce = std::clamp(p, 0.0f, 1.0f);
        header->setSubtitle(
            fmt::format("{:.0f}%", draft->rumbleForce * 100.0f));
    });
    content->addView(slider);
}

void addMouseSpeedSlider(brls::Box* content, StreamConfigProfile* draft) {
    auto* header = new brls::Header();
    header->setTitle("streaming/mouse_speed"_i18n);
    {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1)
               << (0.5f + draft->mouseSpeedMultiplier / 100.0f * 2.5f);
        header->setSubtitle(stream.str() + "x");
    }
    header->setMarginTop(60.0f);
    content->addView(header);

    auto* slider = new brls::Slider();
    slider->setHeight(84.0f);
    slider->setProgress(
        std::clamp(draft->mouseSpeedMultiplier / 100.0f, 0.0f, 1.0f));
    slider->getProgressEvent()->subscribe([draft, header](float p) {
        draft->mouseSpeedMultiplier =
            std::clamp(static_cast<int>(p * 100.0f), 0, 100);
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1)
               << (0.5f + draft->mouseSpeedMultiplier / 100.0f * 2.5f);
        header->setSubtitle(stream.str() + "x");
    });
    content->addView(slider);
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

void addRcasCell(brls::Box* content, StreamConfigProfile* draft) {
    auto* cell = new BooleanSliderCell();
    cell->init("settings/rcas"_i18n, draft->rcas, [draft, cell](bool on) {
        draft->rcas = on;
        cell->setSliderVisibility(on ? brls::Visibility::VISIBLE
                                     : brls::Visibility::GONE);
    });
    cell->setProgress(std::clamp(draft->rcasStrength, 0.0f, 1.0f));
    cell->setValueText(
        fmt::format("{:.0f}%", draft->rcasStrength * 100.0f));
    cell->setSliderVisibility(draft->rcas ? brls::Visibility::VISIBLE
                                          : brls::Visibility::GONE);
    cell->getProgressEvent()->subscribe([draft, cell](float p) {
        draft->rcasStrength = std::clamp(p, 0.0f, 1.0f);
        cell->setValueText(
            fmt::format("{:.0f}%", draft->rcasStrength * 100.0f));
    });
    content->addView(cell);
}

std::string heightLabel(int height) {
    return fmt::format("{}p", height);
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
    content->setWidth(10000);
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

    addHeader(content, "settings/quality"_i18n);

    auto* fpsCell =
        addDetail(content, "settings/fps"_i18n, fmt::format("{} FPS", draft->fps));
    fpsCell->registerClickAction([draft, fpsCell](brls::View*) {
        const std::vector<int> values = {30, 40, 60, 90, 120};
        std::vector<std::string> labels;
        int selected = 2;
        for (size_t i = 0; i < values.size(); ++i) {
            labels.push_back(fmt::format("{} FPS", values[i]));
            if (values[i] == draft->fps)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/fps"_i18n, labels,
            [draft, fpsCell, values](int index) {
                if (index >= 0 && index < static_cast<int>(values.size()))
                    draft->fps = values[static_cast<size_t>(index)];
                fpsCell->setDetailText(fmt::format("{} FPS", draft->fps));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    auto* resCell =
        addDetail(content, "settings/resolution"_i18n,
                  heightLabel(draft->resolutionHeight));
    resCell->registerClickAction([draft, resCell](brls::View*) {
        const std::vector<int> heights = {360, 480, 540, 720, 1080, 1440};
        std::vector<std::string> options;
        int selected = 3;
        for (size_t i = 0; i < heights.size(); ++i) {
            options.push_back(heightLabel(heights[i]));
            if (heights[i] == draft->resolutionHeight)
                selected = static_cast<int>(i);
        }
        auto* dropdown = new brls::Dropdown(
            "settings/resolution"_i18n, options,
            [draft, resCell, heights](int index) {
                if (index >= 0 && index < static_cast<int>(heights.size()))
                    draft->resolutionHeight = heights[static_cast<size_t>(index)];
                resCell->setDetailText(heightLabel(draft->resolutionHeight));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

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

#ifdef SUPPORT_HDR
    addBool(content, "settings/request_hdr"_i18n, draft->requestHdr,
            [draft](bool v) { draft->requestHdr = v; });
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

    addHeader(content, "settings/image_adjustments"_i18n);
    addDitheringCell(content, draft);
    auto* upscaleCell =
        addDetail(content, "settings/upscaling"_i18n, upscalingLabel(draft->upscalingMode));
    upscaleCell->registerClickAction([draft, upscaleCell](brls::View*) {
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
            [draft, upscaleCell](int index) {
                draft->upscalingMode =
                    index == 1   ? UPSCALING_FSR1
                    : index == 2 ? UPSCALING_SGSR1
                    : index == 3 ? UPSCALING_NIS
                                 : UPSCALING_OFF;
                upscaleCell->setDetailText(upscalingLabel(draft->upscalingMode));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    addRcasCell(content, draft);

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

    addHeader(content, "settings/stream_settings"_i18n);
    addBool(content, "settings/usops"_i18n, draft->sops,
            [draft](bool v) { draft->sops = v; });
    addBool(content, "settings/paop"_i18n, draft->playAudioOnPc,
            [draft](bool v) { draft->playAudioOnPc = v; });
    auto* audioCell =
        addDetail(content, "settings/stream_audio_configuration"_i18n,
                  draft->streamAudioConfiguration == STREAM_AUDIO_51_SURROUND
                      ? "5.1"
                      : "Stereo");
    audioCell->registerClickAction([draft, audioCell](brls::View*) {
        const std::vector<std::string> options = {"Stereo", "5.1"};
        auto* dropdown = new brls::Dropdown(
            "settings/stream_audio_configuration"_i18n, options,
            [draft, audioCell](int index) {
                draft->streamAudioConfiguration =
                    index == 1 ? STREAM_AUDIO_51_SURROUND
                               : STREAM_AUDIO_STEREO;
                audioCell->setDetailText(index == 1 ? "5.1" : "Stereo");
            },
            draft->streamAudioConfiguration == STREAM_AUDIO_51_SURROUND ? 1
                                                                        : 0);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    addBool(content, "settings/terminate_app_on_disconnect"_i18n,
            draft->terminateAppOnDisconnect,
            [draft](bool v) { draft->terminateAppOnDisconnect = v; });
    addBool(content, "artemis/settings/force_full_range"_i18n,
            draft->forceFullRangeVideo,
            [draft](bool v) { draft->forceFullRangeVideo = v; });
    addBool(content, "artemis/settings/prevent_packet_loss"_i18n,
            draft->preventPacketLoss,
            [draft](bool v) { draft->preventPacketLoss = v; });

    addHeader(content, "settings/keys"_i18n);
    addBool(content, "settings/swap_ui_ab"_i18n, draft->swapUiAb,
            [draft](bool v) { draft->swapUiAb = v; });
    addBool(content, "settings/swap_ui_xy"_i18n, draft->swapUiXy,
            [draft](bool v) { draft->swapUiXy = v; });

    addHeader(content, "settings/deadzone/title"_i18n);
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
    addHeader(content, "settings/single_joycon"_i18n);
    addBool(content, "settings/swap_stick_to_dpad"_i18n, draft->swapStickToDpad,
            [draft](bool v) { draft->swapStickToDpad = v; });

    addHeader(content, "settings/keyboard"_i18n);
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

    addHeader(content, "settings/mouse"_i18n);
    addBool(content, "settings/touchscreen_mouse_mode"_i18n,
            draft->touchscreenMouseMode,
            [draft](bool v) { draft->touchscreenMouseMode = v; });
    addBool(content, "settings/swap_mouse_keys"_i18n, draft->swapMouseKeys,
            [draft](bool v) { draft->swapMouseKeys = v; });
    addBool(content, "settings/swap_mouse_scroll"_i18n, draft->swapMouseScroll,
            [draft](bool v) { draft->swapMouseScroll = v; });
    addBool(content, "settings/swap_mouse_sticks"_i18n, draft->swapMouseSticks,
            [draft](bool v) { draft->swapMouseSticks = v; });
    addMouseSpeedSlider(content, draft);

    auto* saveCell = addDetail(content, "common/confirm"_i18n, "");
    saveCell->registerClickAction(
        [draft, profileId, assignHostKey, onChanged](brls::View*) {
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
            // Keep live Settings in sync for every field the editor exposes.
            store.applyProfile(*draft);
            delete draft;
            // Pop first so list refresh callbacks see the correct stack.
            brls::Application::popActivity(brls::TransitionAnimation::FADE,
                                           [onChanged] {
                                               if (onChanged)
                                                   onChanged();
                                           });
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
    frame->registerAction("common/cancel"_i18n, brls::BUTTON_B,
                          [draft](brls::View*) {
                              delete draft;
                              brls::Application::popActivity();
                              return true;
                          });
    brls::Application::pushActivity(new brls::Activity(frame));
}

} // namespace artemis::streaming

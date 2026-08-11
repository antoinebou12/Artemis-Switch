#include "ProfileEditorDialog.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <fmt/format.h>
#include <utility>
#include <vector>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

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

std::string heightLabel(int height) {
    return fmt::format("{}p", normalizeProfileHeight(height));
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
    content->setPadding(24, 24, 24, 24);

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

    auto* resCell =
        addDetail(content, "settings/resolution"_i18n, heightLabel(draft->resolutionHeight));
    resCell->registerClickAction([draft, resCell](brls::View*) {
        const std::vector<std::string> options = {"360p", "480p", "720p", "1080p"};
        int selected = 2;
        switch (normalizeProfileHeight(draft->resolutionHeight)) {
        case 360:
            selected = 0;
            break;
        case 480:
            selected = 1;
            break;
        case 1080:
            selected = 3;
            break;
        default:
            selected = 2;
            break;
        }
        auto* dropdown = new brls::Dropdown(
            "settings/resolution"_i18n, options,
            [draft, resCell](int index) {
                constexpr int heights[] = {360, 480, 720, 1080};
                if (index >= 0 && index < 4)
                    draft->resolutionHeight = heights[index];
                resCell->setDetailText(heightLabel(draft->resolutionHeight));
            },
            selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

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

    auto* bitrateCell = addDetail(
        content, "settings/video_bitrate"_i18n,
        fmt::format("{:.1f} Mbps", draft->bitrateKbps / 1000.0));
    bitrateCell->registerClickAction([draft, bitrateCell](brls::View*) {
        brls::Application::getImeManager()->openForNumber(
            [draft, bitrateCell](long number) {
                const int mbps =
                    std::clamp(static_cast<int>(number), 1, 100);
                draft->bitrateKbps = mbps * 1000;
                bitrateCell->setDetailText(
                    fmt::format("{:.1f} Mbps", draft->bitrateKbps / 1000.0));
            },
            "artemis/settings/exact_bitrate_title"_i18n,
            "artemis/settings/exact_bitrate_hint"_i18n, 3,
            std::to_string(std::max(1, draft->bitrateKbps / 1000)), "", "", 0);
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

    addBool(content, "settings/request_hdr"_i18n, draft->requestHdr,
            [draft](bool v) { draft->requestHdr = v; });
    addBool(content, "artemis/settings/force_full_range"_i18n,
            draft->forceFullRangeVideo,
            [draft](bool v) { draft->forceFullRangeVideo = v; });
    addBool(content, "artemis/settings/prevent_packet_loss"_i18n,
            draft->preventPacketLoss,
            [draft](bool v) { draft->preventPacketLoss = v; });

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
    addBool(content, "settings/paop"_i18n, draft->playAudioOnPc,
            [draft](bool v) { draft->playAudioOnPc = v; });
    addBool(content, "settings/usops"_i18n, draft->sops,
            [draft](bool v) { draft->sops = v; });

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
            if (onChanged)
                onChanged();
            delete draft;
            brls::Application::popActivity();
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

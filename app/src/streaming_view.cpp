//
//  streaming_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#ifdef __SWITCH__
#include <borealis/platforms/switch/switch_input.hpp>
#endif

#include "streaming_view.hpp"
#include "AVFrameHolder.hpp"
#include "InputManager.hpp"
#include "click_gesture_recognizer.hpp"
#include "helper.hpp"
#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include "two_finger_scroll_recognizer.hpp"
#include "features/input/InputSettingsStore.hpp"
#include "features/apollo/ApolloHostOptionsStore.hpp"
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#include "features/input/SwitchMotionPolicyStore.hpp"
#include "features/video/ZoomPanStore.hpp"
#include "video/VideoScaleStore.hpp"
#include "features/ui/StatsOverlayLayout.hpp"
#include "streaming/StreamConfigProfileStore.hpp"
#include "streaming/StreamProfileStore.hpp"
#include "streaming/StreamUiLifecycle.hpp"
#include "utils/ArtemisPlatformFeatures.hpp"
#include "utils/UsableMac.hpp"
#include <Limelight.h>
#include <chrono>
#include <filesystem>
#include <nanovg.h>
#include <vector>

#if defined(__SDL3__)
#include <SDL3/SDL.h>
#elif defined(__SDL2__)
#include <SDL2/SDL.h>
#endif

using namespace brls;

namespace {
const char* kProfileSessionBackupSuffix = ".profile-session.bak";

std::vector<std::filesystem::path> globalSettingsFiles() {
    const auto dir =
        std::filesystem::path(Settings::instance().working_dir());
    return {
        dir / "settings.json",
        dir / "artemis_advanced_stream.json",
        dir / "artemis_video_scale.json",
        dir / "artemis_stream.json",
        dir / "artemis_input.json",
        dir / "artemis_zoom_pan.json",
        dir / "artemis_motion.json",
    };
}

void backupGlobalSettingsFiles() {
    std::error_code ec;
    for (const auto& path : globalSettingsFiles()) {
        const auto bak = path.string() + kProfileSessionBackupSuffix;
        std::filesystem::remove(bak, ec);
        if (std::filesystem::exists(path, ec))
            std::filesystem::copy_file(
                path, bak, std::filesystem::copy_options::overwrite_existing,
                ec);
    }
}

void restoreGlobalSettingsFiles() {
    std::error_code ec;
    for (const auto& path : globalSettingsFiles()) {
        const auto bak = path.string() + kProfileSessionBackupSuffix;
        if (std::filesystem::exists(bak, ec)) {
            std::filesystem::copy_file(
                bak, path, std::filesystem::copy_options::overwrite_existing,
                ec);
            std::filesystem::remove(bak, ec);
        }
    }
}
} // namespace

#ifdef PLATFORM_TVOS
extern void updatePreferredDisplayMode(bool streamActive);
#endif

void setBottomBarStatus(const char *value) {
#if defined(__SDL2__) || defined(__SDL3__)
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, value);
#endif
}

void overrideButtonsIfNeeded(bool value) {
#ifdef PLATFORM_SWITCH
    ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setScreenshotButtonOverrideMode(ButtonOverrideMode::NONE);
    ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setHomeButtonOverrideMode(ButtonOverrideMode::NONE);
    if (!value) return;

    switch (Settings::instance().get_overlay_system_button()) {
        case ButtonOverrideType::NONE: break;
        case ButtonOverrideType::HOME:
            ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setHomeButtonOverrideMode(ButtonOverrideMode::CUSTOM_EVENT);
            break;  
        case ButtonOverrideType::SCREENSHOT:
            ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setScreenshotButtonOverrideMode(ButtonOverrideMode::CUSTOM_EVENT);
            break;
    }

    switch (Settings::instance().get_guide_system_button()) {
        case ButtonOverrideType::NONE: break;
        case ButtonOverrideType::HOME:
            ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setHomeButtonOverrideMode(ButtonOverrideMode::GUIDE_BUTTON);
            break;  
        case ButtonOverrideType::SCREENSHOT:
            ((SwitchInputManager*) brls::Application::getPlatform()->getInputManager())->setScreenshotButtonOverrideMode(ButtonOverrideMode::GUIDE_BUTTON);
            break;
    }
#endif
}

StreamingView::StreamingView(const Host& host, const AppInfo& app) : host(host), app(app) {
    Application::getPlatform()->disableScreenDimming(true);

    setFocusable(true);
    setHideHighlight(true);
    loader = new LoadingOverlay(this);

    keyboardHolder = new Box(Axis::COLUMN);
    keyboardHolder->detach();
    keyboardHolder->setJustifyContent(JustifyContent::FLEX_END);
    keyboardHolder->setAlignItems(AlignItems::STRETCH);
    addView(keyboardHolder);

    session = new MoonlightSession(host.preferred_address(), app.app_id,
                                   app.app_uuid);

#if ARTEMIS_END_STREAM_ON_FOCUS_LOSS
    // Switch sleep/HOME: subscribe once here, not in onFocusGained (that fires
    // when overlays close). Disabled on desktop so alt-tab does not kill streams.
    windowFocusSubscription =
        Application::getWindowFocusChangedEvent()->subscribe(
            [this](bool focused) { this->onWindowFocusChanged(focused); });
#endif

#if ARTEMIS_CLEAR_RUMBLE_ON_STREAM_START
    // Clear any rumble left from wireless pads connected before launch.
    clearControllerRumble();
#endif

#ifdef PLATFORM_TVOS
        updatePreferredDisplayMode(true);
#endif

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host, [ASYNC_TOKEN](GSResult<SERVER_DATA> result) {
            ASYNC_RELEASE
            if (!result.isSuccess()) {
                showError(result.error(), [this]() { terminate(false); });
                return;
            }

            session->set_address(
                GameStreamClient::instance().active_address(this->host));

            const auto hostKey = is_usable_mac(result.value().mac)
                                     ? result.value().mac
                                     : this->host.preferred_address();
            auto& profileStore =
                artemis::streaming::StreamConfigProfileStore::instance();
            const auto selectedProfileId =
                profileStore.selectedForHost(hostKey);
            if (!selectedProfileId.empty()) {
                backupGlobalSettingsFiles();
                profileStore.applyToSettings(selectedProfileId, false);
                appliedProfileToRuntime = true;
            }

            artemis::apollo::ApolloHostOptions stored =
                artemis::apollo::ApolloHostOptionsStore::instance().get(hostKey);
            int profileWidth = Application::windowWidth;
            int profileHeight = Application::windowHeight;
            if (auto named = profileStore.get(selectedProfileId);
                named && named->resolutionHeight > 0) {
                profileWidth = named->resolutionWidth();
                profileHeight = named->resolutionHeight;
            } else {
                const auto customProfile =
                    artemis::streaming::StreamProfileStore::instance().get();
                if (customProfile.customResolutionEnabled) {
                    profileWidth = customProfile.width;
                    profileHeight = customProfile.height;
                } else if (Settings::instance().resolution() > 0) {
                    profileHeight = Settings::instance().resolution();
                    profileWidth = artemis::streaming::streamWidthFromHeight(
                        profileHeight, Settings::instance().aspect_ratio());
                }
            }
            stored = artemis::apollo::validateApolloHostOptions(stored);
            const auto profile = artemis::apollo::resolveVirtualDisplay(
                stored, profileWidth, profileHeight);
            APOLLO_LAUNCH_OPTIONS launch;
            launch.virtualDisplay = profile.enabled && !this->app.input_only;
            launch.appUuid = this->app.app_uuid;
            launch.width = profile.width;
            launch.height = profile.height;
            launch.refreshRate = profile.refreshRate;
            launch.scaleFactor = stored.scaleFactor;
            session->setApolloLaunchOptions(std::move(launch));

            ASYNC_RETAIN
            session->start([ASYNC_TOKEN](GSResult<bool> result) {
                ASYNC_RELEASE

                loader->setHidden(true);
                if (!result.isSuccess()) {
                    showError(result.error(), [this]() { terminate(false); });
                }
            }, result.value().isSunshine());
        });

    MoonlightInputManager::instance().reloadButtonMappingLayout();

    static bool lMouseKeyGate = false;
    static bool lMouseKeyUsed = false;
    addGestureRecognizer(new FingersGestureRecognizer([](){
                             return Settings::instance().get_keyboard_fingers();
                         }, [this] { addKeyboard(); }));

    addGestureRecognizer(
        new ClickGestureRecognizer(1, [](TapGestureStatus status) {
            const auto& pointer = artemis::input::InputSettingsStore::instance().pointer();
            if (!artemis::input::isTrackpadMode(pointer.mode) || !pointer.tapToClick) return;

            if (status.state == brls::GestureState::END) {
                Logger::debug("Left mouse click");
                MoonlightInputManager::leftMouseClick();
                lMouseKeyGate = true;
                delay(200, [] { lMouseKeyGate = false; });
            }
        }));

    addGestureRecognizer(
        new ClickGestureRecognizer(2, [](TapGestureStatus status) {
            const auto& pointer = artemis::input::InputSettingsStore::instance().pointer();
            if (!artemis::input::isTrackpadMode(pointer.mode) ||
                !pointer.twoFingerRightClick) return;

            if (status.state == brls::GestureState::END) {
                Logger::debug("Right mouse click");
                MoonlightInputManager::rightMouseClick();
            }
        }));

    addGestureRecognizer(new PanGestureRecognizer(
        [this](PanGestureStatus status, Sound* sound) {
            static bool overlayTriggered = false;

            // Close keyboard by swiping outside of it
            if (status.state == brls::GestureState::START) {
                removeKeyboard();
                overlayTriggered = false;
            }

            // Open overlay by swipe from left screen corner
            bool hasControllers = Application::getPlatform()->getInputManager()->getControllersConnectedCount() > 0;
            if (!Settings::instance().disable_overlay_swipe() && !hasControllers &&
                !overlayTriggered && status.state == brls::GestureState::STAY &&
                status.startPosition.x < 100 && status.position.x > 200) {
                overlayTriggered = true;
                auto overlay = new IngameOverlay(this);
                Application::pushActivity(new Activity(overlay));
            }

            if (!artemis::input::isTrackpadMode(
                    artemis::input::InputSettingsStore::instance().pointer().mode)) return;

            if (status.state == brls::GestureState::UNSURE && lMouseKeyGate) {
                lMouseKeyGate = false;
                lMouseKeyUsed = true;
            } else if (status.state == brls::GestureState::START) {
                if (lMouseKeyUsed) {
//                    Logger::debug("Pressed key at {}", status.state);
                    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,
                                           BUTTON_MOUSE_LEFT);
                }
            } else if (status.state == brls::GestureState::STAY) {
                brls::RawMouseState mouseState;
                Application::getPlatform()->getInputManager()->updateMouseStates(&mouseState);
                // Dirty hack to not update pan if mouse left button is pressed, because pan gesture recognizer will append its speed with raw mouse value
                // Need to improve gesture recognizers to determine the input source and ignore it for mouse
                if (!mouseState.leftButton) {
                    MoonlightInputManager::instance().updateTouchScreenPanDelta(
                            status);
                }
            } else if (lMouseKeyUsed) {
//                Logger::debug("Release key at {}", status.state);
                LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,
                                       BUTTON_MOUSE_LEFT);
                lMouseKeyUsed = false;
            }
        },
        PanAxis::ANY));

    scrollTouchRecognizer = new TwoFingerScrollGestureRecognizer(
            [this](TwoFingerScrollState state) {
                const auto& pointer = artemis::input::InputSettingsStore::instance().pointer();
                if (!artemis::input::isTrackpadMode(pointer.mode)) return;

                if (state.state == brls::GestureState::START)
                    this->touchScrollCounter = 0;

                int threshold = int(state.delta.y / 25);
                if (threshold != this->touchScrollCounter) {
                    Logger::debug("Scroll on: {}",
                                  threshold - this->touchScrollCounter);
                    int invert = (Settings::instance().swap_mouse_scroll() ? -1 : 1) *
                                 (pointer.naturalScrolling ? -1 : 1);
                    char scrollCount = threshold - this->touchScrollCounter;
                    LiSendScrollEvent(scrollCount * invert * pointer.scrollSensitivity);
                    this->touchScrollCounter = threshold;
                }
            });
    addGestureRecognizer(scrollTouchRecognizer);

    keysSubscription =
        Application::getPlatform()
            ->getInputManager()
            ->getKeyboardKeyStateChanged()
            ->subscribe([this, host, app](brls::KeyState state) {
                if (state.key == BRLS_KBD_KEY_ESCAPE) {
                    static std::chrono::high_resolution_clock::time_point
                        clock_counter;
                    static bool buttonState = false;
                    static bool used = false;

                    auto duration =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() -
                            clock_counter);

                    if (!buttonState && state.pressed) {
                        buttonState = true;
                        clock_counter =
                            std::chrono::high_resolution_clock::now();
                    } else if (buttonState && !state.pressed) {
                        buttonState = false;
                        used = false;
                    } else if (buttonState && duration.count() >= 2 && !used) {
                        used = true;

                        auto overlay = new IngameOverlay(this);
                        Application::pushActivity(new Activity(overlay));
                    }
                }
            });
}

void StreamingView::onFocusGained() {
    Box::onFocusGained();

    if (terminated)
        return;

    MoonlightInputManager::instance().setInputEnabled(true);

    if (!blocked) {
        blocked = true;
        Application::blockInputs(true);
    }

    tempInputLock = true;
    ASYNC_RETAIN
    delay(300, [ASYNC_TOKEN]() {
        ASYNC_RELEASE
        this->tempInputLock = false;
    });

    Application::getPlatform()->getInputManager()->setPointerLock(true);

    overrideButtonsIfNeeded(true);
    setBottomBarStatus("1");

    scrollTouchRecognizer->forceReset();
}

void StreamingView::onFocusLost() {
    Box::onFocusLost();

    MoonlightInputManager::instance().setInputEnabled(false);
    MoonlightInputManager::instance().dropInput();

    releaseInputBlock();

    removeKeyboard();
    Application::getPlatform()->getInputManager()->setPointerLock(false);

    overrideButtonsIfNeeded(false);
    setBottomBarStatus("2");

    if (bottombarDelayTask != -1)
        cancelDelay(bottombarDelayTask);
}

void StreamingView::draw(NVGcontext* vg, float x, float y, float width,
                         float height, Style style, FrameContext* ctx) {
    // Once terminate() starts, skip all GPU/input/overlay work so Host menu
    // navigation cannot race deko3d teardown (Switch orange-screen crash).
    if (terminated)
        return;

#if ARTEMIS_END_STREAM_ON_FOCUS_LOSS
    if (pendingSuspendTerminate) {
        // Focus callback only records intent; tear down here on the main loop.
        pendingSuspendTerminate = false;
        terminate(Settings::instance().terminate_app_on_disconnect());
        return;
    }
#endif

    if (session->is_terminated()) {
        terminate(false);
        return;
    }

    // Present against the live Switch framebuffer size so dock↔handheld
    // changes always full-blit even if the AppletFrame layout briefly lags.
#if defined(PLATFORM_SWITCH)
    const int presentW =
        Application::windowWidth > 0 ? Application::windowWidth : (int)width;
    const int presentH =
        Application::windowHeight > 0 ? Application::windowHeight : (int)height;
    session->draw(vg, presentW, presentH);
#else
    session->draw(vg, (int)width, (int)height);
#endif

    if (!tempInputLock && session->is_active())
        handleInput();
    handleOverlayCombo();
    handleMouseInputCombo();

    if (session->connection_status_is_poor()) {
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgFontBlur(vg, 3);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "\uE140 Bad connection...", nullptr);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "\uE140 Bad connection...", nullptr);
    }

    if (session->use_hdr() != m_use_hdr) {
        m_use_hdr = session->use_hdr();

#ifdef PLATFORM_TVOS
        updatePreferredDisplayMode(true);
#endif
    }

    if (draw_stats) {
        auto stats = session->session_stats();

        auto statistics = fmt::format(
                    "Estimated host PC frame rate: {:.{}f} FPS\n"
                        "Incoming frame rate from network: {:.{}f} FPS\n"
                        "Decoding frame rate: {:.{}f} FPS\n"
                        "Rendering frame rate: {:.{}f} FPS\n",
                    stats->video_decode_stats.current_host_fps, 2,
                    stats->video_decode_stats.current_received_fps, 2,
                    stats->video_decode_stats.current_decoded_fps, 2,
                    stats->video_render_stats.rendered_fps, 2);

        statistics += fmt::format("Frames dropped by your network connection: {}\n"
                                  "Average receive time: {:.{}f} | {:.{}f} ms\n"
                                  "Average decode time: {:.{}f} | {:.{}f} ms\n"
                                  "Average decoder delay: {:.{}f} | {:.{}f} ms\n"
                                  "Average rendering time: {:.{}f} ms\n",
                                  stats->video_decode_stats.network_dropped_frames,
                                  stats->video_decode_stats.current_receive_time, 2,
                                  stats->video_decode_stats.session_receive_time, 2,
                                  stats->video_decode_stats.current_decoding_time, 2,
                                  stats->video_decode_stats.session_decoding_time, 2,
                                  stats->video_decode_stats.current_decoder_delay, 2,
                                  stats->video_decode_stats.session_decoder_delay, 2,
                                  stats->video_render_stats.rendering_time, 2);

        if (stats->video_render_stats.gpu_timed_frames > 0) {
            statistics += fmt::format("Average GPU render time: {:.{}f} ms\n",
                                      stats->video_render_stats.gpu_rendering_time, 2);
        }

        if (stats->video_render_stats.post_processed_frames > 0) {
            statistics += fmt::format(
                "Average post-processing pass time: {:.{}f} ms (D:{:.{}f} | U:{:.{}f} | S:{:.{}f})\n",
                /* "Post-processed frames: {} / {}\n", */
                stats->video_render_stats.post_processing_time, 2,
                stats->video_render_stats.dithering_time, 2,
                stats->video_render_stats.upscaling_time, 2,
                stats->video_render_stats.sharpening_time, 2
                /*stats->video_render_stats.post_processed_frames, */
                /*stats->video_render_stats.rendered_frames*/);
        }

        statistics += fmt::format("Frames queue underflows | skipped: {} | {}\n"
                                  "Queue empty | startup holds: {} | {}\n"
                                  "Queue overflow | paced skips: {} | {}\n"
                                  "Scheduled frame holds: {}\n"
                                  "Frames presented by local clock: {}\n"
                                  "Playout resyncs | estimated source: {} | {:.2f} FPS\n"
                                  "Max pushes between draws: {}\n"
                                  "Frames queue depth | target | capacity: {} | {} | {}",
                                  AVFrameHolder::instance().getFakeFrameStat(),
                                  AVFrameHolder::instance().getFrameDropStat(),
                                  AVFrameHolder::instance().getFrameQueueEmptyStat(),
                                  AVFrameHolder::instance().getFrameQueueRebufferHoldStat(),
                                  AVFrameHolder::instance().getFrameQueueOverflowDropStat(),
                                  AVFrameHolder::instance().getFrameQueuePacingSkipStat(),
                                  AVFrameHolder::instance().getFrameQueueScheduledHoldStat(),
                                  AVFrameHolder::instance().getFrameQueueLocalClockPacedFrameStat(),
                                  AVFrameHolder::instance().getFrameQueuePlayoutResyncStat(),
                                  AVFrameHolder::instance().getFrameQueueEstimatedSourceFps(),
                                  AVFrameHolder::instance().getFrameQueueMaxPushBurstStat(),
                                  AVFrameHolder::instance().getFrameQueueSize(),
                                  AVFrameHolder::instance().getFrameQueueTargetDepth(),
                                  AVFrameHolder::instance().getFrameQueueCapacity());

        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgFontSize(vg, 20);
        const auto origin = artemis::ui::stats_overlay_origin(
            static_cast<artemis::ui::StatsCorner>(
                Settings::instance().get_debug_stats_corner()),
            width, height);
        int align = 0;
        align |= origin.alignRight ? NVG_ALIGN_RIGHT : NVG_ALIGN_LEFT;
        align |= origin.alignBottom ? NVG_ALIGN_BOTTOM : NVG_ALIGN_TOP;
        nvgTextAlign(vg, align);

        const float boxWidth = width - 40;
        nvgFontBlur(vg, 1);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgTextBox(vg, origin.alignRight ? origin.x - boxWidth : origin.x,
                   origin.y, boxWidth, statistics.c_str(), nullptr);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(0, 255, 0, 255));
        nvgTextBox(vg, origin.alignRight ? origin.x - boxWidth : origin.x,
                   origin.y, boxWidth, statistics.c_str(), nullptr);
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

void StreamingView::addKeyboard() {
    if (keyboard)
        return;

    keyboard = new KeyboardView(false);
    keyboardHolder->addView(keyboard);
}

void StreamingView::removeKeyboard() {
    if (!keyboard)
        return;

    keyboard->removeFromSuperView();
    keyboard = nullptr;
    Application::giveFocus(this);
}

void StreamingView::clearControllerRumble() {
    int controllersCount = Application::getPlatform()
                               ->getInputManager()
                               ->getControllersConnectedCount();
    for (int i = 0; i < controllersCount; i++)
        Application::getPlatform()->getInputManager()->sendRumble(i, 0, 0);
}

void StreamingView::onWindowFocusChanged(bool focused) {
#if !ARTEMIS_END_STREAM_ON_FOCUS_LOSS
    (void)focused;
    return;
#else
    if (focused || terminated)
        return;

    // Sleep or HOME took focus. End the stream on the next draw() rather than
    // calling terminate() inside the event iteration.
    Logger::info("StreamingView: window focus lost, will end the stream");
    MoonlightInputManager::instance().setInputEnabled(false);
    MoonlightInputManager::instance().dropInput();
    pendingSuspendTerminate = true;
#endif
}

void StreamingView::restoreGlobalSettingsIfNeeded() {
    if (!appliedProfileToRuntime)
        return;
    appliedProfileToRuntime = false;
    restoreGlobalSettingsFiles();
    Settings::instance().load();
    brls::Application::setSwapABInputKeys(Settings::instance().swap_ui_ab());
    brls::Application::setSwapXYInputKeys(Settings::instance().swap_ui_xy());
    artemis::stream::AdvancedStreamOptionsStore::instance().reload();
    artemis::video::VideoScaleStore::instance().reload();
    artemis::streaming::StreamProfileStore::instance().reload();
    artemis::input::InputSettingsStore::instance().reload();
    artemis::input::SwitchMotionPolicyStore::instance().reload();
    artemis::video::ZoomPanStore::instance().reload();
}

void StreamingView::releaseInputBlock() {
    if (!blocked)
        return;
    blocked = false;
    Application::unblockInputs();
}

void StreamingView::terminate(bool terminateApp) {
    if (terminated)
        return;
    terminated = true;
    pendingTeardownTerminateApp = terminateApp;

    // Host deleted/closed the app: drop the stream input grab immediately so
    // the Applications list is not left with a stuck blockInputs token.
    MoonlightInputManager::instance().setInputEnabled(false);
    MoonlightInputManager::instance().dropInput();
    releaseInputBlock();
    clearControllerRumble();
    if (loader)
        loader->setHidden(true);

    // Do not LiStopConnection / dismiss during this draw() — destroying deko3d
    // mid-frame is the Switch orange-screen crash.
    ASYNC_RETAIN
    delay(1, [ASYNC_TOKEN] {
        ASYNC_RELEASE
        finishTeardown();
    });
}

void StreamingView::finishTeardown() {
    if (teardownStarted)
        return;
    teardownStarted = true;

    if (session)
        session->stop(pendingTeardownTerminateApp);
    restoreGlobalSettingsIfNeeded();
    releaseInputBlock();

    Activity* streamActivity = this->getParentActivity();
    auto stack = Application::getActivitiesStack();
    const bool streamIsTop =
        !stack.empty() && stack.back() == streamActivity;

    // View::dismiss → AppletFrame::popContentView → popActivity always pops
    // the top activity. When overlays sit above the stream, the first dismiss
    // clears one overlay; keep clearing until the stream is top, then exit.
    if (streamIsTop) {
        this->dismiss();
        return;
    }

    this->dismiss([this, streamActivity] {
        auto stack = Application::getActivitiesStack();
        while (stack.size() > 1 && stack.back() != streamActivity) {
            Application::popActivity(TransitionAnimation::NONE);
            stack = Application::getActivitiesStack();
        }
        if (!stack.empty() && stack.back() == streamActivity)
            this->dismiss();
    });
}

void StreamingView::applyVirtualDisplay(
    const artemis::apollo::ApolloHostOptions& requested) {
    const auto server = GameStreamClient::instance().server_data(host);
    const auto options = artemis::apollo::validateApolloHostOptions(requested);
    if (!server.isApollo()) {
        showError("artemis/overlay/apollo_only"_i18n);
        return;
    }
    if (options.target != artemis::apollo::VirtualDisplayTarget::Off &&
        (!server.virtualDisplayCapable ||
         !server.virtualDisplayDriverReady)) {
        showError(!server.virtualDisplayCapable
                      ? "artemis/overlay/virtual_display_unsupported"_i18n
                      : "artemis/overlay/virtual_display_driver_not_ready"_i18n);
        return;
    }

    const std::string hostKey = is_usable_mac(server.mac)
        ? server.mac
        : host.preferred_address();
    auto& store = artemis::apollo::ApolloHostOptionsStore::instance();
    const auto previous = store.get(hostKey);

    int profileWidth = Application::windowWidth;
    int profileHeight = Application::windowHeight;
    auto& profileStore =
        artemis::streaming::StreamConfigProfileStore::instance();
    const auto selectedProfileId = profileStore.selectedForHost(hostKey);
    if (auto named = profileStore.get(selectedProfileId);
        named && named->resolutionHeight > 0) {
        profileWidth = named->resolutionWidth();
        profileHeight = named->resolutionHeight;
    } else {
        const auto custom =
            artemis::streaming::StreamProfileStore::instance().get();
        if (custom.customResolutionEnabled) {
            profileWidth = custom.width;
            profileHeight = custom.height;
        } else if (Settings::instance().resolution() > 0) {
            profileHeight = Settings::instance().resolution();
            profileWidth = artemis::streaming::streamWidthFromHeight(
                profileHeight, Settings::instance().aspect_ratio());
        }
    }

    const auto makeLaunch = [this, profileWidth, profileHeight](
                                const auto& value) {
        const auto resolved = artemis::apollo::resolveVirtualDisplay(
            value, profileWidth, profileHeight);
        APOLLO_LAUNCH_OPTIONS launch;
        launch.virtualDisplay = resolved.enabled && !app.input_only;
        launch.appUuid = app.app_uuid;
        launch.width = resolved.width;
        launch.height = resolved.height;
        launch.refreshRate = resolved.refreshRate;
        launch.scaleFactor = value.scaleFactor;
        return launch;
    };

    MoonlightInputManager::instance().dropInput();
    loader->setHidden(false);
    ASYNC_RETAIN
    session->restartWithApolloOptions(
        makeLaunch(options),
        [ASYNC_TOKEN, options, previous, hostKey, makeLaunch](
            GSResult<bool> result) mutable {
            ASYNC_RELEASE
            if (result.isSuccess()) {
                artemis::apollo::ApolloHostOptionsStore::instance().set(
                    hostKey, options);
                loader->setHidden(true);
                auto* dialog = new Dialog(
                    "artemis/overlay/virtual_display_applied"_i18n);
                dialog->addButton("common/close"_i18n, [] {});
                dialog->open();
                return;
            }

            const std::string applyError = result.error();
            session->restartWithApolloOptions(
                makeLaunch(previous),
                [this, applyError](GSResult<bool> rollback) {
                    loader->setHidden(true);
                    if (rollback.isSuccess()) {
                        showError(
                            "artemis/overlay/virtual_display_rollback"_i18n +
                            "\n\n" + applyError);
                    } else {
                        showError(
                            "artemis/overlay/virtual_display_rollback_failed"_i18n +
                            "\n\n" + applyError + "\n" + rollback.error(),
                            [this] { terminate(false); });
                    }
                });
        });
}

void StreamingView::handleInput() {
    if (!this->focused) {
        MoonlightInputManager::instance().dropInput();
        return;
    }

    if (keyboard) {
        static KeyboardState oldKeyboardState;
        KeyboardState keyboardState = keyboard->getKeyboardState();

        for (int i = 0; i < _VK_KEY_MAX; i++) {
            if (keyboardState.keys[i] != oldKeyboardState.keys[i]) {
                oldKeyboardState.keys[i] = keyboardState.keys[i];
                LiSendKeyboardEvent(
                    keyboard->getKeyCode((KeyboardKeys)i),
                    keyboardState.keys[i] ? KEY_ACTION_DOWN : KEY_ACTION_UP, 0);
            }
        }

        // Drop input if keyboard overlay presented
//        MoonlightInputManager::instance().dropInput();
    }
//    else {
    MoonlightInputManager::instance().handleInput(keyboard != nullptr);
//    }

    if (!Application::currentTouchState.empty()) {
        setBottomBarStatus("2");

        if (bottombarDelayTask != -1)
            cancelDelay(bottombarDelayTask);

        ASYNC_RETAIN
        bottombarDelayTask = delay(3000, [ASYNC_TOKEN]() {
            ASYNC_RELEASE
            setBottomBarStatus("1");
            bottombarDelayTask = -1;
        });
    }
}

void StreamingView::handleOverlayCombo() {
    if (!this->focused)
        return;

    KeyComboOptions options = Settings::instance().overlay_options();

    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateUnifiedControllerState(
        &controller);

    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - clock_counter);

    bool buttonsPressed = true;
    for (auto button : options.buttons) {
        buttonsPressed &= controller.buttons[button];
    }

    if (!buttonState && buttonsPressed) {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    } else if (buttonState && !buttonsPressed) {
        buttonState = false;
        used = false;
    } else if (buttonState && duration.count() >= options.holdTime && !used) {
        used = true;

        auto overlay = new IngameOverlay(this);
        Application::pushActivity(new Activity(overlay));
    }

#ifdef PLATFORM_SWITCH
    static bool oldSystemButtonOverlayPressed = false;
    bool systemButtonOverlayPressed = false;
    if (Settings::instance().get_overlay_system_button() == ButtonOverrideType::HOME)
        systemButtonOverlayPressed |= ((SwitchInputManager*) Application::getPlatform()->getInputManager())->isHomeButtonPressed();

    if (Settings::instance().get_overlay_system_button() == ButtonOverrideType::SCREENSHOT)
        systemButtonOverlayPressed |= ((SwitchInputManager*) Application::getPlatform()->getInputManager())->isScreenshotButtonPressed();

    if (oldSystemButtonOverlayPressed != systemButtonOverlayPressed) {
        oldSystemButtonOverlayPressed = systemButtonOverlayPressed;
        if (systemButtonOverlayPressed) {
            auto overlay = new IngameOverlay(this);
            Application::pushActivity(new Activity(overlay));
        }
    }
#endif
}

void StreamingView::handleMouseInputCombo() {
    if (!this->focused)
        return;

    KeyComboOptions options = Settings::instance().mouse_input_options();
    if (options.buttons.empty())
        return;

    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateUnifiedControllerState(
        &controller);

    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - clock_counter);

    bool buttonsPressed = true;
    for (auto button : options.buttons) {
        buttonsPressed &= controller.buttons[button];
    }

    if (!buttonState && buttonsPressed) {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    } else if (buttonState && !buttonsPressed) {
        buttonState = false;
        used = false;
    } else if (buttonState && duration.count() >= options.holdTime && !used) {
        used = true;

        auto overlay = new StreamingInputOverlay(this);
        Application::pushActivity(new Activity(overlay));
    }
}

void StreamingView::onLayout() {
    Box::onLayout();
    if (loader)
        loader->layout();

    if (keyboardHolder) {
        keyboardHolder->setWidth(getWidth());
        keyboardHolder->setHeight(getHeight());
    }
}

StreamingView::~StreamingView() {
#ifdef PLATFORM_TVOS
    updatePreferredDisplayMode(false);
#endif
    
    Application::getPlatform()->disableScreenDimming(false);
    Application::getPlatform()
        ->getInputManager()
        ->getKeyboardKeyStateChanged()
        ->unsubscribe(keysSubscription);
#if ARTEMIS_END_STREAM_ON_FOCUS_LOSS
    Application::getWindowFocusChangedEvent()->unsubscribe(
        windowFocusSubscription);
#endif
    releaseInputBlock();
    restoreGlobalSettingsIfNeeded();
    if (session) {
        session->stop(Settings::instance().terminate_app_on_disconnect());
        delete session;
        session = nullptr;
    }
    artemis::streaming::markStreamUiClosed();
}

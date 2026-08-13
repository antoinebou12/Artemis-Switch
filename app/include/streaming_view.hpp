//
//  streaming_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include "gestures/fingers_gesture_recognizer.hpp"
#include "keyboard_view.hpp"
#include "loading_overlay.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include <optional>
#include "GameStreamClient.hpp"
#include "MoonlightSession.hpp"
#include "two_finger_scroll_recognizer.hpp"

namespace artemis::apollo {
struct ApolloHostOptions;
}

class StreamingView : public brls::Box {
  public:
    StreamingView(const Host& host, const AppInfo& app);
    ~StreamingView();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onLayout() override;

    void terminate(bool terminateApp);
    void showKeyboard() { addKeyboard(); }
    void applyVirtualDisplay(
        const artemis::apollo::ApolloHostOptions& options);

    bool draw_stats = false;

    Host getHost() { return host; }

    AppInfo getApp() { return app; }

  private:
    Host host;
    AppInfo app;
    MoonlightSession* session = nullptr;
    LoadingOverlay* loader = nullptr;
    Box* keyboardHolder = nullptr;
    KeyboardView* keyboard = nullptr;
    bool blocked = false;
    bool terminated = false;
    bool teardownStarted = false;
    bool appliedProfileToRuntime = false;
    bool tempInputLock = false;
    bool pendingSuspendTerminate = false;
    bool pendingTeardownTerminateApp = false;
    brls::Event<brls::KeyState>::Subscription keysSubscription;
    brls::Event<bool>::Subscription windowFocusSubscription;
    brls::VoidEvent::Subscription windowShouldCloseSubscription;
    int touchScrollCounter = 0;
    size_t bottombarDelayTask = -1;
    bool m_use_hdr = false;
    TwoFingerScrollGestureRecognizer* scrollTouchRecognizer = nullptr;

    void onWindowFocusChanged(bool focused);
    void clearControllerRumble();
    void handleInput();
    void handleOverlayCombo();
    void handleMouseInputCombo();
    void addKeyboard();
    void removeKeyboard();
    void restoreGlobalSettingsIfNeeded();
    void releaseInputBlock();
    void finishTeardown();
};

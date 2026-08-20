# Agent memory

## Learned User Preferences

- Never touch, open PRs against, push to, or change https://github.com/XITRIX/Moonlight-Switch. Upstream Moonlight-Switch is read-only reference; all work stays in this fork (`antoinebou12/Artemis-Switch`) unless the user explicitly names that upstream repo and requests an action in the same message.
- Prefer simple, non-breaking ports of Moonlight-Switch ideas into Artemis-Switch PRs; check what is already on fork `main` before re-implementing.
- For multi-feature batches, prefer sequenced separate PRs on Artemis-Switch rather than one mega-PR.
- Keep Quick Actions focused (keyboard, mouse at the top near keyboard, separate move-window left/right, volume, touch screen); put other controls under More; label host shortcuts by feature (not raw key combos); adapt Command vs Win to host OS; keep swap X/Y and swap A/B as separate options; keep host shortcuts distinct from server commands with short helper text.
- Credit XITRIX/Moonlight-Switch first in About/credits; include Apollo, Artemis Classic, Moonlight, Artemide, and Vibepollo inspiration links. README should separate Host (Vibepollo or Apollo) from Client (Artemis); call out HDR as Vibepollo-only.
- Gate Switch-only or risky features so other platforms and CI keep working; document other-platform build/run notes when needed.
- Keep NRO/app author metadata as `antoinebou12`; prefer Artemis branding in app title, About, paths, and folder names (not Moonlight-Switch).
- Host web-config QR should follow switch-wifi NanoVG batched rendering patterns; avoid per-module NanoVG fills on Switch/deko3d, and do not crash if web-config open fails—still show the QR.
- Stream profiles: 1:1 settings parity with the Settings UI (same layout/sliders) without mutating global Settings; no global Artemis Profiles section; open as a page not a popup; Host ready page uses RB/Y/X quick edit/delete; App list keeps the profile row plus manage import/export via `profile.json`; hide custom-resolution controls when that option is off; FSR presets are profile-only and default Off; ship a small set of 30/60 fps defaults with aspect ratio; warn that 1440p+ may lag; low-latency pacing defaults Off; Fit/Fill/Stretch must fill correctly in handheld, dock, and 720p profiles; keep docked menus at 1080 like original Borealis.
- Connected host UI uses an Applications tab (default, refresh with Y) plus a Host settings tab for web-config and stream profile—not Apollo virtual display or scale factor. Treat Sunshine/Apollo as servers only.
- Prefer a global host OS/device setting; place Debug under Stream Performance after Benchmark.
- On disconnect, stream errors, or deleted host apps, safely tear down the session so the Switch is not left on an orange or bricked screen.

## Learned Workspace Facts

- This repository is `antoinebou12/Artemis-Switch`, a Moonlight-Switch fork focused on Switch-specific streaming UX; the usual local artifact is `dist/nro/Artemis-Switch.nro`.
- Fork default branch for day-to-day work is `main`.
- Local reference clone for working Switch QR NanoVG patterns: a `switch-wifi` checkout alongside this repo.
- Client-feature inspiration repos: `derflacco/moonlight-android` (Artemide) and `Nonary/Vibepollo`; Sunshine/Apollo/Vibepollo are host servers.

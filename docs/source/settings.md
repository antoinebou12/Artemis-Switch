# Settings

Every option in Artemis Switch, with the same labels as the app. Stream profiles are a separate snapshot used at launch; they do not rewrite the Settings page.

Bitrate, codec, resolution, and most stream fields apply on the **next** connection. Overlay Options can change a few presentation and input values while a stream is running.

```{seealso}
[What's new](whats-new.md) · [Stream profiles](profiles.md) · [In-stream overlay](overlay.md) · [Performance & telemetry](performance.md) · [Credits](credits.md)
```

## Where settings live

| Place | What it is |
|---|---|
| **Settings** tab | Global Moonlight client options, then the Artemis **Stream** block |
| **Stream profiles** | Named copy of those stream fields (`profile.json`); assign per host |
| **Host** tab (connected) | Web config QR and which profile this host uses |
| **In-stream overlay** | Quick actions, Options, Performance, Debug |

## Recent controls (quick table)

| Control | Default | Applies | Notes |
|---|---|---|---|
| Low latency pacing | Off | Next stream / Options live | Adaptive deadline + latest-frame-wins |
| Frame queue size | 3 | Next stream | 1–5; LL targets 0–1 buffered |
| Full-range video | Off | Next stream / Options live | Honored on NVTEGRA when forced |
| FSR preset | Off | Profile only | Performance / Balanced / Quality |
| Video bitrate (overlay) | — | Live read | Shows **actual / configured** Mbps |

---

## Language

Language
: UI language. **System** follows the Switch locale. Restart the app after changing it.

---

## Quality

Higher quality needs more decode work. 1080p high bitrate usually wants CPU/GPU overclock. **1440p** and Native **2.0x** can stutter; drop to 1080p or 720p if it lags.

FPS
: Stream frame rate requested from the host (typical steps 30 / 40 / 60 / 90 / 120). The Artemis **Frame rate** row is the same value when you use a profile.

Resolution
: Stream width × height sent to Sunshine/Apollo. **Native** follows the Switch output. **1440p (may lag)** is 2560×1440 at 16:9. Custom sizes live under Artemis **Use custom resolution** (up to 2560×1440).

Aspect ratio
: **16:9** or **4:3**. Changes how the requested resolution is interpreted with the scale mode.

Resolution scale
: Extra scale factor on the requested stream size (host still encodes the negotiated resolution).

Video codec
: **H.264**, **HEVC (H.265)**, or **AV1 (Experimental)**. Switch has no hardware AV1 in this build; AV1 may fail to open. Prefer H.264 or HEVC.

Request HDR Video (Vibepollo)
: Ask the host for HDR (10-bit HEVC/AV1). Use with **[Vibepollo](https://github.com/Nonary/Vibepollo)** — that host handles HDR best in testing. Other hosts may ignore or mishandle the request.

Decoder Threads
: Software-decode thread count. **0 (No use threads)** leaves it to FFmpeg. Hardware decode ignores this.

Enable hardware acceleration
: Use Switch NVDEC when the codec allows it. Leave on unless you are debugging decode.

---

## Video bitrate

Video bitrate
: Target encode bitrate in Mbps. Moonlight/GameStream negotiates during setup; Artemis saves the slider for the **next** stream. The Artemis **Exact bitrate** row sets the same value as a number (1–100 Mbps).

---

## Image Adjustments

Dithering
: Reduce banding on gradients. Slider when enabled.

Upscaling (FSR1)
: Run an upscaler after decode (FSR1 / SGSR1 / NIS, depending on build). Helps when the stream is below the Switch output size.

Upscaling mode
: Which upscaler to use when upscaling is on.

RCAS sharpening (FSR)
: Contrast-adaptive sharpen after upscale. Too high looks crunchy.

These four also appear in the in-stream **Options** tab so you can tweak without leaving the game.

---

## Stream

Audio driver
: Audio output backend on the Switch.

Use Streaming Optimal Playable Settings
: Apply Moonlight’s “optimal” preset for the current resolution/FPS. Overrides some quality fields.

Play Audio on PC
: Also play audio on the host while streaming.

Stream audio channels
: **Stereo** or **5.1 surround (downmixed on Switch)**. Surround is mixed down on the client.

Quit host app on disconnect
: Terminate the streamed app on the PC when you use overlay **Disconnect**. Off leaves the app running after a manual disconnect. **Closing or restarting Artemis** (HOME exit, language restart, process quit) always stops the stream and cancels the host app, even when this is Off.

Allow volume amplification
: Let the overlay volume slider go above 100%. Also on overlay **Options** (live; Quick volume max is 100 or 500).

---

## Host device

Host device
: **Windows**, **macOS**, or **Linux**. Changes host-shortcut labels (Win vs Command vs Super) and which keyboard combos Quick Actions send. This is a global OS/device setting, not per host.

---

## Controller keys

Swap A/B for UI
: Swap A and B in Artemis menus only. Separate from in-game mapping.

Swap X/Y for UI
: Swap X and Y in Artemis menus only.

Game keys mapping
: In-game pad layout. **Xbox (Default)** keeps Xbox positions. **Switch (Swap A/B and X/Y)** matches Nintendo face buttons. You can create extra layouts and bind each button.

---

## Dead zones

Left stick / Right stick
: Ignore stick motion below this percent so drift does not move the camera or cursor.

---

## Rumble force

Rumble force
: Scale host rumble to the Switch motors (0–100%).

---

## Single Joycon

Use Stick as D-Pad
: Map the remaining stick to D-pad when using a single Joy-Con.

---

## Guide key (clicks immediately)

Buttons combination
: Button chord that sends the Xbox Guide / host Guide click.

Use system button
: Optional Home or Screenshot button as Guide (cannot share a system button with overlay).

---

## Ingame overlay

Hold to open in seconds
: How long to hold the overlay chord. **0 (Immediately)** opens on press. Default chord is **−** + **+** (or hold Escape on a USB keyboard).

Buttons combination
: Chord that opens the overlay.

Use system button
: Optional Home or Screenshot as overlay (must not collide with Guide).

Debug stats position
: Corner for on-stream debug stats when debugging is on: top/bottom × left/right.

Disable swipe to open overlay
: Ignore the screen-edge swipe that opens the overlay.

---

## Mouse input mode

Hold time / Buttons combination
: Chord that enters mouse-input mode (touch + sticks act as a mouse). Same idea as the overlay chord.

---

## Keyboard

Keyboard type
: On-screen keyboard: **Artemis** (compact), **Full-sized**, or **Numbers & symbols**.

Keyboard layout
: Key labels / locale for the on-screen keyboard (includes **Switch keyboard language**).

Taps to open keyboard
: How many simultaneous fingers open the keyboard (default three-finger tap). Also in overlay Options.

---

## Mouse

Touchscreen mode
: Treat the touch screen as a mouse (move cursor, tap to click). Overlay **Touch screen** can turn this off for the current stream.

Swap mouse ZR/ZL
: Swap left/right click when using ZR/ZL as mouse buttons while touching.

Swap mouse scroll
: Invert scroll direction.

Swap mouse sticks
: Swap which stick moves the cursor vs which stick scrolls.

Mouse speed
: Cursor speed multiplier. Also on overlay **Quick** (compact 84px slider after Mouse).

---

## Debug (Settings)

Show host web config
: Show the Sunshine/Apollo web-config row and QR on the connected Host tab.

Show Performance tab
: When on (default), the stream overlay includes the Performance tab. Turn off to hide it.

Write log
: Write a log file for troubleshooting.

---

## Artemis Stream

This block is at the bottom of Settings. It only has Switch/Artemis extras — FPS, bitrate, resolution, and codec stay in **Quality** above. Named stream profiles do **not** write these global Settings.

### Custom resolution

Use custom resolution
: Ignore the Quality resolution picker and send width × height below. Width/height rows hide when this is off.

Custom width / Custom height
: Exact stream size (width 640–2560, height 360–1440). 1440p may lag.

### Frame Rate & Video

Full-range video
: Request full-range (0–255) instead of limited (16–235). Match what the host encodes. Also on overlay **Options** (live).

Packet-loss guard
: When packet size is Auto/1392, use **1024** bytes for low-MTU or VPN links. Sunshine may still clamp packet size on the host.

Low latency pacing
: Adaptive present-deadline gate with aggressive latest-frame-wins. Uses measured render/GPU cost instead of a fixed 6 ms lead. **Off by default**; can stutter more on unstable Wi-Fi. Also on overlay **Options**.

Frame queue size
: How many decoded frames to keep (1–5, default 3). Low latency pacing still targets at most one buffered frame.

Packet size
: RTP payload size: **Auto** (1392, or 1024 with packet-loss guard), a preset, or **Custom** (200–65535 bytes).

### Presentation

Video scale mode
: How decoded video is drawn: **Fit** (letterbox), **Fill** (crop), **Stretch** (distort), plus Zoom/Pan from the overlay.

Mouse speed
: Compact 0.1×–2.0× slider (same 84px track as overlay volume). Also on overlay **Quick** after Mouse.

Zoom / Pan X / Pan Y
: Compact sliders (1.0–4.0× zoom, −1.0–1.0 pan). Same size as each other and as overlay volume. Overlay **Options** writes them live.

Remember Zoom & Pan
: Keep zoom and pan between sessions. Also on overlay **Options**.

Reset Zoom & Pan
: Return to 1.0×, centered.

### Motion

Controller motion
: Forward Joy-Con / controller gyro and accelerometer to the host (Sunshine DS4-style motion on player 1).

Console motion
: Handheld console-motion fallback. **Off by default.** libnx SevenSixAxis may be present, but vectors are not treated as gyro/accel until they are mapped.

### VPN

Enable WireGuard tunnel
: Bring up a WireGuard tunnel from a `.conf` on the SD card before connecting.

Config file path
: Path to that `.conf`.

Tunnel status
: Last known tunnel state (informational).

---

## Stream profiles

See **[Stream profiles](profiles.md)** for preset tables and what is stored in schema 13.

Named stream snapshots that match the Settings stream fields 1:1: quality (including **Native** resolution, **aspect ratio**, and resolution scale), bitrate, image adjustments, stream/audio, controller, keyboard, mouse (**Touchscreen mode** plus pointer mode), plus Artemis extras (custom resolution, full-range, packet size, **frame queue size**, **low latency pacing**, scale mode, **Remember Zoom & Pan**, controller/console motion). Stored as `profile.json` (schema 13). Launch width is derived from profile height plus that profile’s 16:9 / 4:3 setting. **FSR preset** (Off / Performance / Balanced / Quality) is profile-only and default Off; named presets write existing FSR1 (EASU) plus RCAS strength and are greyed out while upscaling is Off. Built-in seeds include **720p 60 Low Latency**. Language, host OS, overlay chords, VPN, and Debug stay in Settings only. Editing or assigning a profile does not change global Settings.

On first launch (empty `profile.json`), Artemis seeds fourteen 30/60 FPS presets (see the [profiles table](profiles.md)). Existing custom profiles are not replaced. Deleting every profile does not auto-reseed. Virtual display and scale stay on the Sunshine/Apollo host web UI.

Default profile
: Global fallback when a host has no assigned profile.

Manage profiles
: Create, rename, duplicate, delete. **Add missing defaults** inserts any built-in preset whose name is not already present.

Export / Import profiles
: Write or read `profile.json`. App list can import/export the same file.

On a connected host:

- **Applications** tab: pick a game; the profile row selects which snapshot to launch with.
- **Host** tab: assign the profile for that host.
- Host ready page: **RB** / **Y** / **X** for quick edit / delete (see on-screen hints).

There is no separate global “Artemis Profiles” settings section. Edit profiles from the host UI or the profile editor page (not a popup).

---

## Add host

Host IP
: Manual address (LAN, Tailscale, or public IP).

Add address
: Extra endpoint for the same host.

Connect
: Pair / connect to the typed address.

Search
: LAN discovery list. Pick a found host instead of typing an IP.

---

## Connected host

### Applications

Game grid plus search. Starting an app uses the selected profile.

Search apps
: Filter the grid by name (**LB**, or the Search row). Does not hit the host.

Refresh apps
: **Y**. Reconnects, then `GET /applist?uniqueid=…` on the Sunshine/Apollo HTTPS port (usually 47984). Use this after adding or renaming apps in the host web UI. Apollo may also include UUID / input-only entries in that XML.

### Host

Host web config
: QR and URL for the Sunshine/Apollo web UI (add apps, see PIN). If QR drawing fails, the URL is still shown.

Stream profile
: Profile assigned to this host, or **Global settings**. New / Edit / Delete / rename live here. Saving a profile does not change the Settings tab.

Wake up
: Send Wake-on-LAN if the host was configured for it.

---

## In-stream overlay

Detailed pages with diagrams (not duplicated here):

| Tab | Page |
|---|---|
| Overview | [In-stream overlay](overlay.md) |
| Quick | [Overlay — Quick](overlay-quick.md) |
| Options | [Overlay — Options](overlay-options.md) |
| Performance | [Overlay — Performance](overlay-performance.md) |
| Credits | [Credits](credits.md) |

Debug (on-stream debug view + log) sits under Performance after Benchmark; see Settings → Debug above.

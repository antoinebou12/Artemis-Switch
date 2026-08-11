# Settings

Every option in Artemis Switch, with the same labels as the app. Stream profiles use the same layout as Settings; assigned profiles override the global defaults for that host.

Bitrate, codec, resolution, and most stream fields apply on the **next** connection. Overlay Options can change a few presentation and input values while a stream is running.

## Where settings live

| Place | What it is |
|---|---|
| **Settings** tab | Global Moonlight client options, then the Artemis **Stream** block |
| **Stream profiles** | Named copy of those stream fields (`profile.json`); assign per host |
| **Host** tab (connected) | Web config QR and which profile this host uses |
| **In-stream overlay** | Quick actions, Options, Performance, Debug |

---

## Language

Language
: UI language. **System** follows the Switch locale. Restart the app after changing it.

---

## Quality

Higher quality needs more decode work. 1080p high bitrate usually wants CPU/GPU overclock.

FPS
: Stream frame rate requested from the host (typical steps 30 / 40 / 60 / 90 / 120). The Artemis **Frame rate** row is the same value when you use a profile.

Resolution
: Stream width × height sent to Sunshine/Apollo. **Native** follows the Switch output. Custom sizes live under Artemis **Use custom resolution**.

Aspect ratio
: **16:9** or **4:3**. Changes how the requested resolution is interpreted with the scale mode.

Resolution scale
: Extra scale factor on the requested stream size (host still encodes the negotiated resolution).

Video codec
: **H.264**, **HEVC (H.265)**, or **AV1 (Experimental)**. Switch has no hardware AV1 in this build; AV1 may fail to open. Prefer H.264 or HEVC.

Request HDR Video
: Ask the host for HDR. Only useful if the host and display path actually output HDR.

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
: Terminate the streamed app on the PC when you disconnect. Off leaves the app running.

Allow volume amplification
: Let the overlay volume slider go above 100%.

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
: Cursor speed multiplier. Also on overlay Options.

---

## Debug (Settings)

Show host web config
: Show the Sunshine/Apollo web-config row and QR on the connected Host tab.

Write log
: Write a log file for troubleshooting.

---

## Artemis Stream

This block is at the bottom of Settings. Stream profiles edit the same fields.

### Custom resolution

Use custom resolution
: Ignore the Quality resolution picker and send width × height below. Width/height rows hide when this is off.

Custom width / Custom height
: Exact stream size (width 640–1920, height 360–1080).

Exact bitrate
: Bitrate in Mbps as a number instead of the Quality slider (1–100).

### Frame Rate & Video

Frame rate
: Same as Quality **FPS**.

Full-range video
: Request full-range (0–255) instead of limited (16–235). Match what the host encodes.

Packet-loss guard
: When packet size is Auto/1392, use **1024** bytes for low-MTU or VPN links. Sunshine may still clamp packet size on the host.

Low latency pacing
: Occupancy / alpha-beta frame pacing aimed at lower input lag. **Off by default**; can stutter more on unstable Wi-Fi.

Packet size
: RTP payload size: **Auto** (1392, or 1024 with packet-loss guard), a preset, or **Custom** (200–65535 bytes).

### Resolution presets

Preset
: Quick sizes: Handheld 1280×720, Docked 1920×1080, 4:3 handheld/docked, or Custom. Applies resolution and aspect; does not replace a named stream profile.

### Apollo & display

These live in Settings and in the stream profile. They are **not** on the Host tab. Launch uses the selected profile.

Apollo virtual display
: Ask Apollo to create a virtual display for this stream: Off, current profile size, handheld, docked, portrait, or custom `WIDTHxHEIGHT@HZ`. Needs Apollo with the virtual-display driver installed. Sunshine hosts ignore this.

Apollo scale factor
: DPI / scale sent to Apollo for that virtual display.

### Presentation

Video scale mode
: How decoded video is drawn: **Fit** (letterbox), **Fill** (crop), **Stretch** (distort), plus Zoom/Pan from the overlay.

Remember Zoom & Pan
: Keep zoom and pan between sessions.

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

Named full settings snapshots with the same sections as Settings (video, presentation, stream, audio, keyboard, mouse, controller, Apollo virtual display / scale). Stored as `profile.json`.

Default profile
: Global fallback when a host has no assigned profile.

Manage profiles
: Create, rename, duplicate, delete.

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

Game grid plus the stream-profile row. Starting an app uses the selected profile.

### Host

Host web config
: QR and URL for the Sunshine/Apollo web UI (add apps, see PIN). If QR drawing fails, the URL is still shown.

Stream profile
: Profile assigned to this host, or **Global settings**. New / Edit / Delete / rename live here.

Wake up
: Send Wake-on-LAN if the host was configured for it.

---

## In-stream overlay — Quick

Keyboard
: Open the on-screen keyboard.

Mouse
: Enter mouse-input mode (next to Keyboard on purpose).

Move window left / right
: Host shortcut to move the focused window to the left or right display. Labels follow **Host device**.

Touch screen
: **On** or **Off** for touch-as-mouse this session.

Host shortcuts
: Keyboard presets (Escape, fullscreen, paste, Start/Command/Super, desktop, Game Bar, task switcher, task manager / Force quit, IME switch, and the same move-window actions). Not the same as server commands.

Restart server
: Matched host command if the host advertises it.

Reset display
: Matched host command if advertised.

Server commands
: Apollo host scripts, distinct from keyboard shortcuts.

Volume
: Client volume. Amplification above 100% requires **Allow volume amplification**.

---

## In-stream overlay — Options

Input overlay
: Extra on-stream input helper.

Keyboard type / Taps to open keyboard
: Same as Settings → Keyboard.

Pointer mode
: How touch/mouse pointer is interpreted (trackpad, gaming trackpad, multi-touch, absolute, swapped absolute, or disabled).

Controllers
: Connected pads, player slots, rumble + motion, rumble tests.

Clipboard
: Apollo-only fetch / edit / upload / paste of host clipboard text.

Video rotation
: Rotate the presented video (portrait virtual displays).

Scale mode
: Same as Settings **Video scale mode**, live.

Low latency pacing
: Same as Settings, toggleable during a stream.

Zoom / Pan X / Pan Y
: Manual framing. Reset returns to 1.0× centered.

Guide key
: Same chord / system-button options as Settings.

Mouse speed
: Same as Settings.

Image Adjustments
: Dithering, upscaling, upscaling mode, RCAS — same as Settings.

---

## In-stream overlay — Performance

Live telemetry for the current stream. Not persistent settings except Debug at the bottom.

| Row | Meaning |
|---|---|
| Bitrate | Configured stream bitrate |
| Switch Wi-Fi | Client Wi-Fi signal; graph under the row |
| Receive / Decode / Render | Stage latencies |
| Packet loss | Reported loss |
| Host / Network / Decoder / Rendered FPS | Pipeline frame rates (graphs on network and decoder) |
| Frame queue | Queued decoded frames |
| Presentation | Current scale / presentation path |
| Mode, CPU / GPU / Memory clock, Battery | Switch runtime |
| Benchmark | Start / stop / save / clear a timing capture |
| Show debugging view | On-stream debug overlay |
| On-screen log | Overlay log lines |

Debug sits under Stream Performance after Benchmark.

---

## Credits for these options

Moonlight-Switch (XITRIX) is the base client. Frame-rate-sync / low-latency pacing is adapted from nyanpasu64’s Moonlight-Switch work ([#323](https://github.com/XITRIX/Moonlight-Switch/issues/323)). Apollo virtual display and related host commands follow Apollo / Artemis Classic behavior.

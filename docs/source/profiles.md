# Stream profiles

Named snapshots of stream settings stored in `profile.json` (schema **13**). Profiles are selected per host and applied at launch. Editing or assigning a profile does **not** rewrite the global Settings tab.

```{seealso}
Full field list: [Settings](settings.md). Live metrics while streaming: [Performance & telemetry](performance.md).
```

## How profiles work

| Step | What happens |
|---|---|
| Create / edit | Opens a full Settings-style **page** (not a popup). Same layout and sliders as Settings. |
| Assign on Host tab | That host uses the profile on the next stream start. |
| Launch an app | Profile fields are applied with `persist=false` so global Settings files stay untouched. |
| Import / export | Read or write `profile.json` from Manage profiles or the app list. |

| Stored in profile | Stays in Settings only |
|---|---|
| Resolution, aspect, FPS, bitrate, codec, HDR, decoder threads, HW decode | Language |
| Scale mode, zoom/pan remember, upscaling, dither, RCAS, **FSR preset** | Host OS / device |
| Full-range, packet-loss guard, packet size, **low latency pacing**, **frame queue size** | Overlay chords |
| Audio, keyboard, mouse/pointer, controller, motion toggles | VPN / WireGuard |
| Custom resolution | Debug |

## Built-in presets

On first launch (empty `profile.json`), Artemis seeds these 30/60 FPS presets. **Add missing defaults** inserts any name that is not already present. Existing custom profiles are never replaced.

| Name | Height | FPS | Bitrate | Notes |
|---|---:|---:|---:|---|
| 360p 30 0.5M | 360 | 30 | 0.5 Mbps | Very low bandwidth |
| 360p 30 1M | 360 | 30 | 1 Mbps | |
| 480p 30 5M | 480 | 30 | 5 Mbps | |
| 480p 60 10M | 480 | 60 | 10 Mbps | |
| 540p 30 5M | 540 | 30 | 5 Mbps | |
| 540p 60 10M | 540 | 60 | 10 Mbps | |
| 720p 30 10M | 720 | 30 | 10 Mbps | |
| 720p 60 10M | 720 | 60 | 10 Mbps | Default active profile |
| 720p 60 20M | 720 | 60 | 20 Mbps | |
| **720p 60 Low Latency** | 720 | 60 | 10 Mbps | Low latency pacing **On**, queue size **2** |
| 1080p 30 20M | 1080 | 30 | 20 Mbps | |
| 1080p 60 20M | 1080 | 60 | 20 Mbps | |
| 1080p 60 50M | 1080 | 60 | 50 Mbps | |
| 1080p 60 100M | 1080 | 60 | 100 Mbps | High bandwidth |
| **1440p 30 20M** | 1440 | 30 | 20 Mbps | May lag on Switch |
| **1440p 30 50M** | 1440 | 30 | 50 Mbps | May lag on Switch |
| **1440p 60 50M** | 1440 | 60 | 50 Mbps | May lag on Switch |
| **1440p 60 100M** | 1440 | 60 | 100 Mbps | May lag on Switch |

1440p is **2560×1440** at 16:9. The Switch panel is 720p/1080p, so the extra pixels are decode cost. Settings shows a warning for 1440p and Native **2.0x**. Existing installs: **Add missing defaults** on Manage profiles.

Seeds use **16:9**. The editor can switch a profile to **4:3**. Virtual display and Apollo scale stay on the host web UI, not in the profile.

## New profile / Settings fields

| Control | Default | Where | What it does |
|---|---|---|---|
| **Low latency pacing** | Off | Settings, profile, overlay Options | Adaptive present deadline + latest-frame-wins. Lower lag; may stutter on bad Wi-Fi. |
| **Frame queue size** | 3 (1–5) | Settings, profile | Decoded frames kept before present. LL mode still targets at most one buffered frame. |
| **Full-range video** | Off | Settings, profile, Options | Request 0–255 range. On Switch/NVTEGRA, Force Full Range is honored (JPEG quirk only when limited was requested). |
| **FSR preset** | Off | Profile only | Off / Performance / Balanced / Quality → FSR1 + RCAS strength. Greyed when upscaling is Off. |

## Host UI cheatsheet

| Place | Action |
|---|---|
| **Applications** tab | Pick a game; profile row selects the snapshot for launch. |
| **Host** tab | Assign profile for this host; web-config QR. |
| Host ready page | **RB** / **Y** / **X** quick edit / delete (on-screen hints). |
| Manage profiles | Create, rename, duplicate, delete, add missing defaults, import/export. |

There is no separate global “Artemis Profiles” section in Settings.

## Recommended stacks

| Role | Recommendation |
|---|---|
| Host | Vibepollo or Apollo (Sunshine as compatibility baseline) |
| Switch client | This app (Artemis Switch) |
| Android client | Artemide (FSR presets inspired this fork) |

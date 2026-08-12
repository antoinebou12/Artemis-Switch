# What's new

Highlights from recent Artemis Switch work (profiles schema 13, adaptive low-latency pacing, and client telemetry).

| Area | Change |
|---|---|
| **Low latency pacing** | Adaptive present deadline from measured render/GPU cost; aggressive latest-frame-wins |
| **Frame queue size** | Settings + profile control (1–5); LL still caps target buffer to 0–1 |
| **Telemetry** | Actual vs configured bitrate; R·D·Q·G·C pipeline; queue jitter; startup markers |
| **Benchmark** | µs timing windows, queue/bitrate/startup fields in JSON export |
| **Full-range video** | Force Full Range honored on NVTEGRA (quirk only when limited was requested) |
| **Apollo detection** | Sunshine-safe capability detection via version/fields (no loose OR-only `isApollo`) |
| **Profiles** | Schema 13; seeded **720p 60 Low Latency**; FSR presets remain profile-only |
| **Docs site** | Blue Material theme (IPC Toolkit–style); dedicated Profiles and Performance pages |

```{seealso}
- [Stream profiles](profiles.md)
- [Performance & telemetry](performance.md)
- [Settings reference](settings.md)
```

## Quick start for low latency

| Step | Action |
|---|---|
| 1 | Open a host → assign or edit a profile |
| 2 | Prefer **720p 60 Low Latency**, or turn **Low latency pacing** On |
| 3 | Keep **Frame queue size** at 2–3 unless you need more buffering |
| 4 | Stream, open overlay **Performance**, watch actual bitrate and R·D·Q·G·C |
| 5 | Run **Benchmark** for a few minutes and save the report if comparing Wi-Fi vs Ethernet |

## Not in this release

| Topic | Status |
|---|---|
| Console-motion vector remapping | Still Off by default; not expanded in the telemetry PR |
| Removing encoded-frame `memcpy` before decode | Investigate-only; stability kept |
| Switch thread-priority tuning | Not changed |

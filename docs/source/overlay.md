# In-stream overlay

While a stream is running, open the overlay to reach **Quick**, **Options**, **Performance**, and Debug.

```{image} _static/diagrams/overlay-overview.svg
:alt: Overlay tabs and how they connect to the stream and host
:align: center
:width: 100%
```

| Tab | Purpose | Detail page |
|---|---|---|
| **Quick** | Focused actions: keyboard, mouse, move window, volume, host shortcuts | [Quick](overlay-quick.md) |
| **Options** | Live presentation and input: scale, zoom/pan, LL pacing, full-range, image filters | [Options](overlay-options.md) |
| **Performance** | Telemetry: bitrate, pipeline R·D·Q·G·C, queue, benchmark | [Performance](overlay-performance.md) |
| **Debug** | On-stream debug overlay and log (under Performance after Benchmark) | [Settings → Debug](settings.md) |

```{image} _static/diagrams/overlay-storage.svg
:alt: Class diagram of overlay tabs vs Settings and profile storage
:align: center
:width: 100%
```

| Writes live | Read-only |
|---|---|
| Quick → mouse speed, volume, host key/scripts | Performance → decode/render/queue stats |
| Options → scale, zoom/pan, LL pacing, full-range, filters | Benchmark → samples until you save |

Profiles apply at **launch** (`persist=false`). Overlay Options can still change a subset **during** the stream without rewriting the Settings page.

```{seealso}
[Credits](credits.md) · [Stream profiles](profiles.md) · [Performance & telemetry](performance.md)
```

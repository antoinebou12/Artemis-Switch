# Performance & telemetry

The in-stream **Performance** tab and the **Benchmark** export show where latency comes from on the Switch client. Metrics refresh about every **250 ms** of real time (steady clock), not on accumulated decode milliseconds.

```{seealso}
Low latency pacing and frame queue size: [Stream profiles](profiles.md) and [Settings](settings.md).
```

## Pipeline (receive → present)

```text
Host encode → network arrival [T0]
            → NVTEGRA decode  [T1]
            → frame queue
            → present select  [T2]
            → Deko3D draw     [T3]
```

| Stage | Meaning |
|---|---|
| **Receive (R)** | Reassembly / receive time from Moonlight decode stats |
| **Decode (D)** | Steady-clock decode duration (µs precision, shown in ms) |
| **Queue (Q)** | Wait between decode done and frame selected for present |
| **Render (G)** | Client draw / submit cost |
| **Client (C)** | End-to-end client pipeline **T3 − T0** |

The Performance latency row formats as:

`R · D · Q · G · C ms`

## Live Performance rows

| Row | Shows |
|---|---|
| **Video bitrate** | `actual / configured` Mbps. Actual is measured from encoded frame sizes over ~250 ms. Configured is the Settings bitrate. |
| **Receive / pipeline** | Combined R·D·Q·G·C latency text |
| **Decode / Render / GPU** | Individual decode and render costs |
| **Packet loss** | Recent network drop percent |
| **Host / Received / Decoded / Rendered FPS** | FPS chain + graphs |
| **Frame queue** | `depth / target` and jitter ms when low-latency pacing is active |
| **Presentation** | Scale mode · Full or Limited range |
| **Switch clocks / battery** | Read-only runtime metadata (when available) |
| **Benchmark** | Start / stop / save / reset; summary score on stop |

## What each number tells you

| Symptom | Likely cause |
|---|---|
| Jitter ↑, received FPS unstable, network drops ↑ | Wi-Fi / network |
| Decode p95 ↑, decoder delay ↑ | Decoder / resolution / codec load |
| Queue depth ↑ while render FPS OK | Presentation / pacing backlog |
| GPU render p95 ↑ | Deko3D / upscaling cost |
| Actual bitrate ≪ configured | Host not filling the requested bitrate |

## Low latency pacing

| Mode | Target buffer | Present behavior |
|---|---|---|
| **Balanced** (pacing Off) | About 1–2 frames | Legacy occupancy / frame-credit pacing |
| **Low latency** (pacing On) | 0–1 frames | Adaptive deadline from render+GPU p95 + safety (~1.5–8 ms lead), plus aggressive **latest-frame-wins** |

Until enough present-cost samples exist, the gate uses a short default lead (~3 ms), not a fixed 6 ms forever.

Inspired by Moonlight-Switch issue [#323](https://github.com/XITRIX/Moonlight-Switch/issues/323) (algorithm credit; Artemis implementation is opt-in).

## Startup timing

Once per stream, telemetry records:

| Marker | Meaning |
|---|---|
| Time to first packet | Stream start → first encoded unit |
| Time to first decode | Stream start → first decoded frame |
| Time to first present | Stream start → first Deko3D present |

Benchmark sampling waits until the **first successful present** so decoder warm-up is not scored as a degraded session.

## Benchmark export fields

| Field group | Examples |
|---|---|
| FPS | host, received, decoded, rendered (mean / p95 / p99) |
| Latency | receive, decode, decoder delay, render, GPU, client pipeline, queue wait |
| Queue | depth, jitter, underflows, overflow drops, pacing skips, resyncs |
| Network | drop %, **actual video Mbps** |
| Startup | time to first packet / decode / present |
| Score | stability 0–100 |

Exports include Switch runtime metadata (operation mode, clocks, battery) when available. Clock queries are **read-only**.

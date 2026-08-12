# Overlay — Performance

Live client telemetry and benchmark controls. Hidden when Settings → Debug → **Show Performance tab** is off (default **on**).

```{image} _static/diagrams/telemetry-loop.svg
:alt: Activity diagram of the 250 ms performance and benchmark sample loop
:align: center
:width: 100%
```

Samples use ~**250 ms** of real elapsed time. Benchmark sampling waits until the **first successful present** so decoder warm-up is not scored as a bad session.

## Rows

| Row | Shows |
|---|---|
| **Video bitrate** | `actual / configured` Mbps |
| **Receive latency** | Pipeline **R · D · Q · G · C** ms |
| Decode / Render / GPU | Stage costs |
| Packet loss | Recent network drop % |
| Host / Received / Decoded / Rendered FPS | FPS chain + graphs |
| Frame queue | `depth / target` (+ jitter ms in LL mode) |
| Presentation | Scale mode · Full or Limited |
| Mode / clocks / battery | Switch runtime (read-only) |
| Benchmark | Start / stop / save / reset |

Debug (on-stream debug view + log) sits **under** Performance after Benchmark.

## Reading the pipeline

| Letters | Stage |
|---|---|
| **R** | Receive / reassembly |
| **D** | Decode (steady-clock µs → ms) |
| **Q** | Queue wait (decode done → present select) |
| **G** | Render / GPU submit |
| **C** | Client end-to-end (present − network complete) |

| Symptom | Likely cause |
|---|---|
| Jitter ↑, received FPS unstable | Wi-Fi / network |
| Decode p95 ↑ | Decoder / resolution / codec |
| Queue depth ↑, render OK | Presentation / pacing backlog |
| GPU p95 ↑ | Deko3D / upscaling |
| Actual ≪ configured bitrate | Host not filling requested bitrate |

More detail: [Performance & telemetry](performance.md).

## Credits for Performance

| Source | Credit |
|---|---|
| [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Base decode/render stats overlay |
| [#323](https://github.com/XITRIX/Moonlight-Switch/issues/323) [@nyanpasu64](https://github.com/nyanpasu64) | Pacing stats / low-latency queue thinking |
| This fork | Actual Mbps, R·D·Q·G·C, queue jitter, startup markers, benchmark export |

```{seealso}
[Options](overlay-options.md) · [Credits](credits.md) · [Performance & telemetry](performance.md)
```

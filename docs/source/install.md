# Install

Use a file from a [GitHub Release](https://github.com/antoinebou12/Artemis-Switch/releases/latest) or from `dist/nro` after a local Switch build.

## Easy install

Extract `Artemis-Switch-SD.zip` to the **root** of the Switch SD card. That ZIP is only the ready-to-copy layout:

```text
switch/Artemis-Switch/Artemis-Switch.nro
```

## Manual install

Copy `Artemis-Switch.nro` to:

```text
sdmc:/switch/Artemis-Switch/Artemis-Switch.nro
```

## Launch

1. Open the Homebrew Menu with **title redirection** (full RAM). On stock Atmosphere that is usually hold **R** and start a game.
2. Select **Artemis Switch**.
3. Pair Sunshine or Apollo with the normal Moonlight/GameStream flow.

Applet mode is not supported for reliable streaming.

```{warning}
High bitrate at 1080p often needs CPU/GPU overclocking (for example sys-clk). Overclocking is outside Artemis Switch and is at your own risk.
```

## Host software

Apollo and Sunshine both use the Moonlight/GameStream pairing flow. Artemis does not replace the host; it is the Switch client.

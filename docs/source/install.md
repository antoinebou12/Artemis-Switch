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
3. Pair the host with the normal Moonlight/GameStream flow.

Applet mode is not supported for reliable streaming.

```{warning}
High bitrate at 1080p often needs CPU/GPU overclocking (for example sys-clk). Overclocking is outside Artemis Switch and is at your own risk.
```

## Host software

Sunshine, Apollo, Vibeshine, and Vibepollo use the normal Moonlight/GameStream pairing flow. Their web console is normally at `https://<host>:47990/`.

Punktfunk is native-only by default. Enable its GameStream compatibility plane using the official [Moonlight guide](https://docs.punktfunk.unom.io/docs/moonlight), then open `https://<host>:47992/` to enter Artemis's pairing PIN. Artemis sends the selected resolution and FPS through GameStream, and Punktfunk creates the matching virtual display.

Artemis does not implement Punktfunk's native protocol, call authenticated host-management endpoints, or store host administration tokens.

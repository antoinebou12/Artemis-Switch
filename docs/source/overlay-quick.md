# Overlay — Quick

Focused in-stream actions. Keep this tab small: keyboard, mouse, move-window, volume, touch, and host shortcuts. Put everything else under [Options](overlay-options.md).

```{image} _static/diagrams/overlay-quick.svg
:alt: Quick Actions flow to keyboard, mouse, host shortcuts, and Apollo commands
:align: center
:width: 100%
```

## Controls

| Control | What it does |
|---|---|
| **Keyboard** | Open the on-screen keyboard |
| **Mouse** | Enter mouse-input mode (placed next to Keyboard on purpose) |
| **Mouse speed** | Compact 0.1×–2.0× slider (84px track), directly under Mouse |
| **Move window left / right** | Host shortcut to move the focused window. Labels follow **Host device** (Win / Command / Super) |
| **Touch screen** | On/Off touch-as-mouse for this session |
| **Host shortcuts** | Keyboard presets (Escape, fullscreen, paste, Start/Command/Super, desktop, Game Bar, task switcher, task manager / Force quit, IME switch, move-window). Not the same as server commands |
| **Restart server** | Matched host command if the host advertises it |
| **Reset display** | Matched host command if advertised |
| **Server commands** | Apollo host scripts (clipboard / custom commands). Distinct from keyboard shortcuts |
| **Volume** | Client volume. Above 100% needs **Allow volume amplification** in Options/Settings |

## Design rules

| Do | Don't |
|---|---|
| Keep Quick focused on frequent actions | Put filters, scale, or Debug here |
| Label host shortcuts by feature | Show raw key combos as the primary label |
| Keep host shortcuts ≠ server commands | Mix Apollo scripts into keyboard presets |
| Separate swap X/Y and swap A/B (in Settings) | Collapse face-button swaps into one toggle |

## Credits for Quick

| Source | Credit |
|---|---|
| [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Base overlay / Borealis streaming UI |
| Apollo / Artemis Classic | Server commands and clipboard-style host actions |
| This fork | Mouse next to keyboard; separate move-window left/right; OS-aware shortcut labels |

```{seealso}
[Options](overlay-options.md) · [Credits](credits.md)
```

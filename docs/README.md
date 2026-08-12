# Docs site

GitHub Pages site for Artemis Switch, in the same shape as [IPC Toolkit](https://ipctk.xyz): **blue** Material theme, top tabs, search, and a GitHub link.

**Live:** <https://antoinebou12.github.io/Artemis-Switch/>

| Page | File |
|---|---|
| Home | `source/index.md` |
| What's new | `source/whats-new.md` |
| Install | `source/install.md` |
| Settings (every option) | `source/settings.md` |
| Stream profiles | `source/profiles.md` |
| Performance & telemetry | `source/performance.md` |

## Build locally

```bash
pip install -r docs/requirements.txt
sphinx-build -b html docs/source docs/_build/html
```

Open `docs/_build/html/index.html`.

Pushes to `main` that touch `docs/` deploy via `.github/workflows/docs.yml`.

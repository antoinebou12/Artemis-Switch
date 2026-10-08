# Running CI locally with `act`

[`act`](https://github.com/nektos/act) runs the repository's GitHub Actions
workflows in Docker on your machine, so a change can be checked against the
same jobs CI runs before it is pushed.

```bash
scripts/ci-local.sh          # Git Bash, WSL, Linux, macOS
scripts\ci-local.ps1         # PowerShell (calls the bash script)
```

## What it runs

| Workflow | Job | What it checks |
| --- | --- | --- |
| `unit-tests.yml` | `portable-tests` | i18n + release contracts, then the whole unit suite in **Release** |
| `unit-tests.yml` | `portable-sanitizers` | the unit suite in Debug under ASan + UBSan |
| `feature-integration-ci.yml` | `portable-integration` | contracts (incl. the VPN build contract), unit suite in Debug, cross-feature integration suite |
| `feature-integration-ci.yml` | `sanitizer-integration` | sanitizer run of the unit suite |

Pick jobs with `-j workflow:job` (repeatable), list them with `--list`, and keep
the staged tree for inspection with `--keep`.

The Switch NRO build (`docker-image.yml:build`) is not in the default set because
it is a full devkitA64 build (slow, and it needs several GB of disk). It is untested
under `act`; the same recipe is verified via `scripts/run-build.ps1`:

```bash
scripts/ci-local.sh -j docker-image.yml:build
```

## What it tests

The script stages the current `HEAD` commit **plus every uncommitted change**
(modified, deleted and new files) into a throw-away clone, fetches the four
shallow submodules the test jobs need, and points `act` at that. So you test what
is in your working tree now, and after a merge the same command tests `main`.

It deliberately does not run `act` against the working tree directly:

- `act --bind` would reuse your `build/` directories, whose CMake caches record
  host paths and make CMake refuse to configure inside the container.
- Copying the working tree would pull in `extern/vcpkg` (about 11,700 files) and
  read every file from OneDrive.

## Requirements

- Docker (running), `act` 0.2.x, and `git`.
- On Windows, Git for Windows' bash.
- The first run builds `artemis-act:ubuntu-latest` from `ci/act/Dockerfile`: the
  stock `act-latest` runner image plus `cmake`, `ninja` and `rsync`, which
  GitHub's hosted `ubuntu-latest` has but the stock image lacks. Override the image
  with `ARTEMIS_ACT_IMAGE`, or the binary with `ACT_BIN`.

## Differences from GitHub

- Checkout and artifact upload steps are skipped or replaced (`env.ACT`), so
  there is no artifact retention and no checkout action. The workflows' "Fetch
  submodules" step is skipped for the same reason; the script does the
  equivalent before starting `act`.
- `release-package-contract` (needs a recursive checkout and rsyncs the whole
  tree) and the multi-platform `all-builds.yml` matrix are not run locally.

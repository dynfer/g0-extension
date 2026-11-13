The new fome-fw integration targets the local `upstream/master` revision
`b3ae7a9aa0` and the version 8 extension protocol at `19f0c4c` in this repository.

The implementation is in [modules](modules/), and the six ordered, mergeable
commits are exported in [patches](patches/series). [series.json](series.json)
records the exact base and commit IDs. [ANALYSIS.md](ANALYSIS.md) explains the
existing implementations and the remaining limitations.

The same six commits are available as local branch `codex/g0-modular` in
`/home/denes/fome-fw`. [VALIDATION.md](VALIDATION.md) records the build and test results.

| Merge request | Scope |
| --- | --- |
| 1 | Shared wire contract, pipeline, validation, reconnect, fair polling, host tests |
| 2 | Background SPI transport and opt-in Atlas board wiring |
| 3 | Twelve ADC channels with stale-data handling and sensor filter recovery |
| 4 | Four digital inputs using the existing GPIO interface |
| 5 | Four queued lowside outputs and local 1..500 Hz PWM |
| 6 | Optional startup firmware updater with image validation and flash readback |

The patches are cumulative; merge them in this order. Firmware support is disabled
by default. The base already supplies ADC providers and the background SPI worker,
so those frameworks do not need to be reintroduced. Parts 1..5 require no firmware
image, extension submodule, Java UI changes, or firmware-loader code.

To apply the series in a separate fome-fw worktree:

```sh
git -C /home/denes/fome-fw worktree add -b review/g0-modular /tmp/fome-g0-review upstream/master
git -C /tmp/fome-g0-review am /home/denes/g0-extension/for_fome/patches/*.patch
```

The patch base is recorded exactly; a newer upstream revision may need rebasing.
The source copies in `modules/` correspond to
`firmware/hw_layer/g0_extension/` after all six patches. Existing-file changes,
connector definitions, and build wiring are included in the patches.

Run the standalone host tests from this repository:

```sh
bash for_fome/test.sh
```

The [module documentation](modules/README.md) covers Atlas pin mapping, APIs,
build commands, the optional image generator, timing, and hardware verification.
The normal runtime build is:

```sh
cd /path/to/patched/fome-fw/firmware/config/boards/atlas
G0_EXTENSION=yes bash ./compile_atlas.sh
```

SENT sensor registration and frequency-sensor integration are deferred. The
current extension's SENT decoder does not validate CRCs, and its outputs retain
their last command after communication loss. The host integration cannot provide
a slave-side output watchdog; see the analysis before selecting loads.

The modular fome-fw integration starts at `upstream/master` revision
`b3ae7a9aa0`. The audit follow-up upgrades both ends to protocol 9 with CRCs,
link-loss shutdown and automatic firmware identities; see [AUDIT_FIXES.md](AUDIT_FIXES.md).

The implementation is in [modules](modules/), and the eight ordered commits for six PR parts
are exported in [patches](patches/series). [series.json](series.json)
records the exact base and commit IDs. [ANALYSIS.md](ANALYSIS.md) explains the
existing implementations and the remaining limitations.

The complete integration is available as local branch `codex/g0-modular` in
`/home/denes/fome-fw`. [VALIDATION.md](VALIDATION.md) records the build and test results.
Separate [PR branches](BRANCHES.md) are published to the `dynfer/fome-fw` fork.
The updater branch also preserves the newer upstream merge made on the fork;
the eight exported patches retain the original base for staged integration.

| Merge request | Scope |
| --- | --- |
| 1 | Shared wire contract, pipeline, validation, reconnect, fair polling, host tests |
| 2 | Background SPI transport and opt-in Atlas board wiring |
| 3 | Twelve ADC channels with stale-data handling and sensor filter recovery |
| 4 | Four digital inputs using the existing GPIO interface |
| 5 | Four queued lowside outputs and local 1..500 Hz PWM |
| 6 | Automatic G0 firmware bundling and startup updater with flash readback |

The patches are cumulative; merge them in this order. Atlas enables G0 support
and automatic image bundling by default; `G0_EXTENSION=no` disables both. The base already supplies ADC providers and the background SPI worker,
so those frameworks do not need to be reintroduced. Parts 1..5 require no firmware
image, extension submodule, Java UI changes, or firmware-loader code. The final
audit follow-up coordinates changes across these parts; use the complete series
with the new slave firmware.

To apply the series in a separate fome-fw worktree:

```sh
git -C /home/denes/fome-fw worktree add -b review/g0-modular /tmp/fome-g0-review upstream/master
git -C /tmp/fome-g0-review am /home/denes/g0-extension/for_fome/patches/*.patch
```

The patch base is recorded exactly; a newer upstream revision may need rebasing.
The source copies in `modules/` correspond to
`firmware/hw_layer/g0_extension/` after the complete patch series. Existing-file changes,
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
bash ./compile_atlas.sh
```

SENT is rejected until a validated decoder exists. Registering frequency sensors
with fome-fw remains a separate integration step. Outputs shut off after 500 ms
without a checksum-valid request. Hardware timing and the physical updater path
still need bench verification.

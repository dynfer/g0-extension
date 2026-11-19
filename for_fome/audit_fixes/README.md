Selected G0 audit fixes (2026-11-19)

These four cumulative branches on `dynfer/fome-fw` start at `de73644150`,
the updater branch after its upstream merge. Each adds one commit. Review each
against its predecessor; SPI timeout/recovery changes were explicitly skipped.

| Finding | Branch | Commit | Review base |
| --- | --- | --- | --- |
| 1: companion protocol | [codex/g0-submodule-protocol](https://github.com/dynfer/fome-fw/tree/codex/g0-submodule-protocol) | `d5bac91120` | `de73644150` |
| 3A: pinned ChibiOS | [codex/g0-pinned-chibios](https://github.com/dynfer/fome-fw/tree/codex/g0-pinned-chibios) | `d3d2669bf2` | `codex/g0-submodule-protocol` |
| 4: open-drain reset | [codex/g0-reset-open-drain](https://github.com/dynfer/fome-fw/tree/codex/g0-reset-open-drain) | `0c0d85b134` | `codex/g0-pinned-chibios` |
| 5: unused update mode | [codex/g0-remove-unused-update-mode](https://github.com/dynfer/fome-fw/tree/codex/g0-remove-unused-update-mode) | `0803b735a7` | `codex/g0-reset-open-drain` |

Matching branch names exist on `dynfer/g0-extension`. The ChibiOS check lives in
G0 source commit `e1a3415`; removing the unused update mode advances the FOME
companion pin to `689042c`. Later export documentation does not change that image.
The original module branches and twelve-patch export remain historical snapshots.

1. **Use the checked-out companion wire protocol.** FOME includes the companion's
   canonical header, removing its independent protocol version and frame copy.
   Runtime-only builds initialize the source for this header without building an
   image; disabling the expansion skips both dependencies. Response validation
   still rejects an incompatible running device.
2. **Require clean, pinned ChibiOS sources for G0 firmware identity.** The G0 build
   rejects a different ChibiOS revision, modified tracked files and untracked
   files. Commit driver work and update the submodule pin before building it.
3. **Release G0 NRST with open drain.** RESET retains its latch-before-mode setup,
   but HIGH releases the line with a pull-up so G0 or ST-Link can assert reset.
4. **Remove the unused application update mode.** Delete its flag, accessor,
   handler and LED branch. The command number stays reserved and returns an
   unsupported-command response. Programming still uses BOOT0/NRST.

The four patches here apply with `git am --whitespace=error` from `de73644150`
and reproduce the final FOME branch tree. The module mirror matches it exactly.

Validation: all four host and both slave/version sanitizer test groups pass;
FOME's own module tests pass too. A separate native build uses a companion header
with protocol 1234 and asserts that version in every translation unit, proving
that no FOME version edit is needed. The clean Atlas board script builds and
packages the bootloader, application and companion. The 12,016-byte G0 image
occurs exactly once in `fome.bin`; identity is `0x26fa15cb`. Both compiled images
report protocol 9. The full engine-control suite was not rerun. Nothing was
flashed; physical reset/debug behavior and SPI fault recovery need bench testing.

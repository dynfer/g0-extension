The compact integration is published as six cumulative branches on `dynfer/fome-fw`,
based on `FOME-Tech/fome-fw:master` at `e628773bd0`. Each branch adds one
module to the preceding part; all parts use protocol 9. The updater includes a
follow-up commit for factory BOOT0 provisioning.

| Part | Fork branch | Tip | Review base |
| --- | --- | --- | --- |
| 1 | [codex/g0-v9-01-protocol](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-01-protocol) | `79a891adea` | `upstream/master` |
| 2 | [codex/g0-v9-02-spi-transport](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-02-spi-transport) | `718fb0b349` | `codex/g0-v9-01-protocol` |
| 3 | [codex/g0-v9-03-adc](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-03-adc) | `ea0e3babbf` | `codex/g0-v9-02-spi-transport` |
| 4 | [codex/g0-v9-04-digital-inputs](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-04-digital-inputs) | `4cd04606e8` | `codex/g0-v9-03-adc` |
| 5 | [codex/g0-v9-05-outputs](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-05-outputs) | `52aec6335f` | `codex/g0-v9-04-digital-inputs` |
| 6 | [codex/g0-v9-06-updater](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-06-updater) | `faf5d2cc8b` | `codex/g0-v9-05-outputs` |

The commits cover protocol/link, background SPI transport, ADC, digital inputs,
outputs/PWM, and automatic firmware bundling/updating, respectively. Their fixes
and tests are contained in the owning module.

For incremental review, compare each branch with the review base above. Submit
upstream PRs in this order; a later branch compared against upstream master also
includes its prerequisites until they merge. The final branch builds and bundles
G0 source revision `5fdddd58c6` automatically using the ROM updater. It enables
BOOT0 selection on factory-blank G071 devices before programming main flash.

[codex/g0-option-provisioning](https://github.com/dynfer/fome-fw/tree/codex/g0-option-provisioning)
also names the provisioning snapshot at `e37de65226`. Compare it with the previous
updater tip `0cb6472e53` to review only this follow-up. The updater branch advanced
normally; its original commit and the first five branches were preserved.

The earlier `codex/g0-01-*` through `codex/g0-06-*`, `codex/g0-modular` and
`codex/g0-safety-fixes` branches remain unchanged as historical snapshots. The
compact branches replace the previous eight-patch export without force-pushing
those branches. The user's `ps_idle` checkout and uncommitted files are preserved.

The upload fixes are also published on
[codex/g0-upload-fixes](https://github.com/dynfer/fome-fw/tree/codex/g0-upload-fixes).
Compare against `e37de65226` for post-flash reset, true microsecond byte spacing,
periodic scheduler sleeps and upload LED activity. The final small follow-up
retains the updater's required precompiled-header include order.

[codex/g0-spi5-chip-selects](https://github.com/dynfer/fome-fw/tree/codex/g0-spi5-chip-selects)
contains the CS initialization follow-up at `4bc718d70d`; compare against
`06ea4d7ec5`. The updater branch includes it, while earlier review snapshots
remain unchanged.

[codex/g0-pwm-fixes](https://github.com/dynfer/fome-fw/tree/codex/g0-pwm-fixes)
contains the PWM compatibility fix at `03800b69d6`; compare against `4bc718d70d`.
It supports paused initial/runtime frequencies and propagates global PWM disable
and restore through the SPI worker. The updater includes this follow-up.

[codex/g0-boot-release](https://github.com/dynfer/fome-fw/tree/codex/g0-boot-release)
contains the SWCLK/BOOT0 fix at `020223048f`; compare against `03800b69d6`.
The updater drives BOOT during initialization, then releases it to input/pull-down
on completion so H7 no longer clamps the G0 debug clock. NRST behavior is unchanged.

The updater subsequently merged upstream at `79b594285f`. Build fixes on top of
that merge are exported separately in [build_fixes](build_fixes/README.md):
`codex/build-generated-headers` (`588b8a7a24`) recovers stale or missing generated
headers, and `codex/build-arm-binutils` (`faf5d2cc8b`) uses the selected ARM tools
for packaging. The second branch includes the first; the updater includes both.
The original twelve-patch stack and historical module branches remain unchanged.

The selected 2026-11-19 audit fixes have four separate cumulative branches on top
of `de73644150`: companion-owned protocol, clean pinned ChibiOS, open-drain NRST,
and unused update-mode removal. See [audit_fixes](audit_fixes/README.md) for
commit IDs, review bases and matching G0 source branches. SPI timeout changes
were skipped. Existing updater/module branches were preserved.

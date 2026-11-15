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
| 6 | [codex/g0-v9-06-updater](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-06-updater) | `03800b69d6` | `codex/g0-v9-05-outputs` |

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

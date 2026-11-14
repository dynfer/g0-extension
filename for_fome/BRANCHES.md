The compact integration is published as six cumulative branches on `dynfer/fome-fw`,
based on `FOME-Tech/fome-fw:master` at `e628773bd0`. Each branch adds one
module commit to the preceding part; all parts use protocol 9.

| Part | Fork branch | Tip | Review base |
| --- | --- | --- | --- |
| 1 | [codex/g0-v9-01-protocol](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-01-protocol) | `79a891adea` | `upstream/master` |
| 2 | [codex/g0-v9-02-spi-transport](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-02-spi-transport) | `718fb0b349` | `codex/g0-v9-01-protocol` |
| 3 | [codex/g0-v9-03-adc](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-03-adc) | `ea0e3babbf` | `codex/g0-v9-02-spi-transport` |
| 4 | [codex/g0-v9-04-digital-inputs](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-04-digital-inputs) | `4cd04606e8` | `codex/g0-v9-03-adc` |
| 5 | [codex/g0-v9-05-outputs](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-05-outputs) | `52aec6335f` | `codex/g0-v9-04-digital-inputs` |
| 6 | [codex/g0-v9-06-updater](https://github.com/dynfer/fome-fw/tree/codex/g0-v9-06-updater) | `0cb6472e53` | `codex/g0-v9-05-outputs` |

The commits cover protocol/link, background SPI transport, ADC, digital inputs,
outputs/PWM, and automatic firmware bundling/updating, respectively. Their fixes
and tests are contained in the owning module; there is no audit follow-up to
apply across the stack.

For incremental review, compare each branch with the review base above. Submit
upstream PRs in this order; a later branch compared against upstream master also
includes its prerequisites until they merge. The final branch builds and bundles
G0 source revision `5fdddd58c6` automatically using the existing ROM updater.

The earlier `codex/g0-01-*` through `codex/g0-06-*`, `codex/g0-modular` and
`codex/g0-safety-fixes` branches remain unchanged as historical snapshots. The
compact branches replace the previous eight-patch export without force-pushing
those branches. The user's `ps_idle` checkout and uncommitted files are preserved.

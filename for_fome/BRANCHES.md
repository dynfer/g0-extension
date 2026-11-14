The six integration branches are published to the `dynfer/fome-fw` fork and
tracked locally in `/home/denes/fome-fw`.

| Part | Fork branch | Tip | Previous part / review base |
| --- | --- | --- | --- |
| 1 | [codex/g0-01-protocol](https://github.com/dynfer/fome-fw/tree/codex/g0-01-protocol) | `fd824ebbdf` | `upstream/master` at `b3ae7a9aa0` |
| 2 | [codex/g0-02-spi-transport](https://github.com/dynfer/fome-fw/tree/codex/g0-02-spi-transport) | `f1785a8e5c` | `codex/g0-01-protocol` |
| 3 | [codex/g0-03-adc](https://github.com/dynfer/fome-fw/tree/codex/g0-03-adc) | `94b9e7ffa5` | `codex/g0-02-spi-transport` |
| 4 | [codex/g0-04-digital-inputs](https://github.com/dynfer/fome-fw/tree/codex/g0-04-digital-inputs) | `96da172a9c` | `codex/g0-03-adc` |
| 5 | [codex/g0-05-outputs](https://github.com/dynfer/fome-fw/tree/codex/g0-05-outputs) | `743bfe056c` | `codex/g0-04-digital-inputs` |
| 6 | [codex/g0-06-updater](https://github.com/dynfer/fome-fw/tree/codex/g0-06-updater) | `ae7793cccd` | `codex/g0-05-outputs` |

Parts 1..5 each add one integration commit to the previous part. Part 6 includes
the updater, automatic image bundling and the coordinated protocol-9 audit fixes, on top of the newer
upstream merge already present in that fork branch. The branches are
cumulative because the modules depend on earlier parts. Submit upstream PRs to
`FOME-Tech/fome-fw:master` in this order; until earlier parts merge, later PRs
against upstream master also include those prerequisites. For an incremental
review before merging, compare a branch with the previous part in this table.

The complete audited integration is also published as
[codex/g0-safety-fixes](https://github.com/dynfer/fome-fw/tree/codex/g0-safety-fixes)
at `ae7793cccd`. Its single follow-up commit is based on the previous updater tip
`7164cbba3b`. The G0 companion sources are on
[codex/g0-audit-fixes](https://github.com/dynfer/g0-extension/tree/codex/g0-audit-fixes);
fome-fw pins code revision `5fdddd58c6`.

The existing `codex/g0-modular` branch retains the complete six-part integration
and audit fix on the original base, at `9992d7a852`, matching the eight exported
patches. Parts 1..5 retain their original review snapshots; the complete new
slave requires the coordinated audit follow-up.
Publishing these branches did not switch the user's `ps_idle` checkout or alter
its uncommitted files.

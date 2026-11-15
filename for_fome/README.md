The compact fome-fw integration has six module branches based on
`upstream/master` at `e628773bd0`. Every part uses protocol 9; audit fixes are
included in the module that owns them.

[BRANCHES.md](BRANCHES.md) lists the published fork branches and review bases.
[UPDATER_PR.md](UPDATER_PR.md) contains the updated description for the updater PR.
[series.json](series.json) records the exact commits and pinned G0 source.
The source mirror in [modules](modules/) matches the final branch, and
[patches/series](patches/series) lists the nine patches in merge order. The
seventh patch adds factory BOOT0 provisioning to the updater module; the eighth
adds the post-flash reset, faster byte spacing and cooperative upload progress.
The ninth retains the required precompiled-header include order.

| Part | Scope |
| --- | --- |
| 1 | Shared wire contract, CRC validation, response pipeline and reconnect |
| 2 | Background SPI worker and Atlas wiring |
| 3 | Twelve ADC inputs, expiry and filter recovery |
| 4 | Four digital inputs and acknowledged mode configuration |
| 5 | Four lowsides, local PWM, inversion and Lua frequency changes |
| 6 | Automatic G0 image build/bundling, factory BOOT0 provisioning and verified startup programming |

Each branch contains its preceding parts. The I/O parts opt in with
`G0_EXTENSION=yes`; the final updater part enables Atlas support by default.
`G0_EXTENSION=no` disables it. `G0_FIRMWARE_UPDATE=no` keeps runtime I/O without
building or programming the companion image.

Apply the series in a fresh checkout of the recorded base:

```sh
git am /home/denes/g0-extension/for_fome/patches/*.patch
```

Run both the fome-fw module tests and actual G0 source regressions:

```sh
bash for_fome/test.sh
```

Build Atlas normally from `firmware/config/boards/atlas` with
`bash ./compile_atlas.sh`; no prepared G0 image header is needed.
[VALIDATION.md](VALIDATION.md) records the checks.

The earlier branches remain available as historical snapshots. The compact
stack replaces the older eight-patch series. [ANALYSIS.md](ANALYSIS.md) records
the original branch comparison and [AUDIT_FIXES.md](AUDIT_FIXES.md) explains the
slave safety fixes. Hardware timing and the physical ROM updater still need
bench verification. SENT is unsupported.

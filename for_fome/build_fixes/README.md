These two build fixes apply after the updater's upstream merge at
`79b594285f`. They are separate from the original twelve-patch module export,
which remains reproducible from its recorded upstream base.

| Fix | Branch | Review base | Commit |
| --- | --- | --- | --- |
| Generation recovery | [codex/build-generated-headers](https://github.com/dynfer/fome-fw/tree/codex/build-generated-headers) | `79b594285f` | `588b8a7a24` |
| ARM packaging tools | [codex/build-arm-binutils](https://github.com/dynfer/fome-fw/tree/codex/build-arm-binutils) | `588b8a7a24` | `faf5d2cc8b` |

The updater branch includes both commits. Apply these patches in order to
`79b594285f`, or review each branch against the base shown above.

Generation checks enum inputs and tracked enum headers before compiling, so a
header restored by Git cannot silently bypass cached generation. Missing stage
manifests or generated files trigger their owning stage, including from the
bootloader directory. Generated files are not committed.

Packaging queries `use_arm_gcc.mk` for the selected ARM toolchain and uses its
objcopy/objdump by absolute path. It supports the existing TRGT override and
propagates selection/tool failures; no PATH wrappers are required.

Run the regression checks from the fome-fw repository:

```sh
python3 firmware/tests/test_enum_generation.py
python3 firmware/tests/test_checksum_tools.py
```

Validated on 2026-11-18: both Python regression suites pass, and the ordinary Atlas
board script builds and packages OpenBLT and the main firmware without wrappers.
The cached-pull scenario is checked by replacing the GPIO-name header with the
old tracked version while retaining both build stamps, then rebuilding normally.
No hardware is flashed by these checks.

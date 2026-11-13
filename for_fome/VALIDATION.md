Validation completed on 2026-11-14 against local fome-fw `upstream/master`
`b3ae7a9aa0`, using extension revision `19f0c4c` and application version 8.
The final integration commit is recorded in [series.json](series.json).

| Check | Result |
| --- | --- |
| Extension `make -j12` | Passed; STM32G071 binary produced |
| Full Atlas board script, `G0_EXTENSION=yes G0_FIRMWARE_UPDATE=yes` | Passed; OpenBLT and main firmware linked and packaged |
| Full fome-fw native test build and run | Passed: 749 tests from 136 suites |
| Host module tests at each of the six commit prefixes | Passed with AddressSanitizer and UndefinedBehaviorSanitizer |
| Image generator | Valid image accepted; old version, truncation, oversize, invalid reset vector, and out-of-range stack rejected |
| Exported patch series | Applied with `git am --whitespace=error` to a fresh base checkout; resulting tree matched exactly |
| Source copies in `modules/` | Matched the files in the final fome-fw commit |
| `git diff --check` | Passed |

The native firmware tests use their normal configuration with G0 disabled. They
cover the shared ADC/GPIO paths; the standalone host groups exercise the G0
protocol and feature state machines. Atlas compilation checks the actual SPI,
GPIO, PWM, and updater adapters with G0 enabled. Other production board builds
and physical hardware tests were not run.

The Atlas build used the vendored ARM GCC 11.3 toolchain. Java tooling used the
installed JDK 25 with Java 21 output in an isolated Gradle cache. ARM `objcopy`
and `objdump` were supplied for the board script's checksum step. Host runs kept
AddressSanitizer and UndefinedBehaviorSanitizer enabled and disabled leak checking
because LeakSanitizer cannot run under the execution environment's ptrace setup.

Builds ran in a separate checkout. The user's `ps_idle` checkout, modified VS Code
settings, and untracked extension directory were preserved. The six commits were
imported into `/home/denes/fome-fw` as local branch `codex/g0-modular`; nothing was
pushed or flashed.

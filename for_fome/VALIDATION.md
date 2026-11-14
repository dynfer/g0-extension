Audit fixes validated on 2026-11-14 with companion code `5fdddd58c6`,
protocol 9, and fome-fw `ae7793cccd` (updater branch) / `9992d7a852` (original-base
patch series). [AUDIT_FIXES.md](AUDIT_FIXES.md) maps all 13 findings to fixes and
regression coverage.

| Check | Result |
| --- | --- |
| G0 normal `make -j12` and `for_fome_image` | Passed with system ARM GCC 14.2; generated identity and image header |
| Actual slave source regressions | Passed with AddressSanitizer and UndefinedBehaviorSanitizer |
| Host protocol, production G0 output adapter, bootloader and image-generator suites | Passed with AddressSanitizer and UndefinedBehaviorSanitizer |
| Firmware identity generator | Two Python tests passed: source/compiler/options/dependency changes, stable output and header timestamps |
| Clean normal Atlas board script | Passed with bundled G0 firmware and OpenBLT packaging, using vendored ARM GCC 11.3 |
| Packaged G0 image | All 12,060 bytes found exactly once in `deliver/fome.bin`; `g0_image::data` is in ELF `.rodata` at `0x080d5f50` |
| Bundled identity | Generated firmware ID `0x722cede0` matches the image-header version |
| Repeat normal Atlas build | Passed; G0 binary, firmware-ID header and image-header timestamps unchanged |
| Full native fome-fw tests | Passed: 770 tests from 137 suites |
| Eight exported patches | Applied with `git am --whitespace=error` from the recorded base; tree matches the original-base branch |
| Module source mirrors | Match both integration branches byte for byte |

Leak detection was disabled because LeakSanitizer cannot run under ptrace;
AddressSanitizer and UndefinedBehaviorSanitizer remained enabled. Builds used an
isolated checkout and writable Gradle cache, preserving the user's checkout and
unrelated VS Code settings. No hardware was flashed. Input interrupt latency,
physical startup levels, PWM timing, SPI recovery, BOOT option bytes and the ROM
programming path still need bench validation. SENT is rejected.

The following records describe the earlier protocol-8 integration and bundling
validation, before the audit fixes above.

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
flashed. The six [PR branches](BRANCHES.md) have since been published to the fork.

The automatic bundling follow-up on the updater branch also passed these checks:

| Check | Result |
| --- | --- |
| Normal Atlas script without G0 flags or a prepared image header | Passed; builds the pinned companion and embeds it |
| Full G0 image in packaged `fome.bin` | Verified all 12,304 bytes; ELF places `g0_image::data` in read-only flash |
| Repeat normal build | Passed; G0 binary and image-header timestamps remain unchanged |
| Clean build after deleting generated outputs and deinitializing the G0 submodule | Passed; restores pinned sources, rebuilds G0, and regenerates the embedded header |
| Host protocol and image-generator tests | Passed; existing protocol groups plus five Python test cases |
| G0-disabled and updater-disabled make paths | Passed with the companion source directory absent |

This follow-up adds build dependencies and image generation; it does not change
the runtime protocol or programming sequence. The full 749-test native result
above is from the original integration validation.

The fork updater branch had received upstream merge `15aa40ba80` during the work.
The bundling change was reapplied on top as `7164cbba3b`, preserving that merge.
A clean normal Atlas build on the resulting branch passed, and the complete G0
image was again verified in the packaged binary. The exported seven-patch series
and `codex/g0-modular` retain the original base; the G0 module sources match the
updated PR branch.

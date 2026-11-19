Validated the original compact stack on 2026-11-14, based on `upstream/master`
`e628773bd0`. Its six commits and branch names are recorded in
[series.json](series.json); its updater tip was `0cb6472e53` before the provisioning
follow-up at `e37de65226`.

| Check | Result |
| --- | --- |
| Protocol branch | Standalone tests passed with AddressSanitizer and UndefinedBehaviorSanitizer |
| SPI transport branch | Standalone tests and Atlas build passed |
| ADC branch | Standalone tests and clean Atlas build passed |
| Digital-input branch | Standalone tests and clean Atlas build passed |
| Output/PWM branch | Model and actual output-adapter tests plus clean Atlas build passed |
| Updater branch | Protocol, bootloader, output-adapter and Python image-generator tests plus clean default Atlas build passed |
| All branch prefixes | Use protocol 9; each adds one module commit to its predecessor |
| Actual G0 slave regression suite and firmware identity tests | Passed with sanitizers |
| Full native fome-fw suite | 770 tests from 137 suites passed |
| Bundled G0 image | All 12,060 bytes found exactly once in packaged `fome.bin` |
| Read-only image storage | ELF `g0_image::data` at `0x080a345c`, in `.rodata` |
| Image identity | Bundled header matches G0 build identity `0x722cede0` |
| Repeat default Atlas build | G0 binary, identity header and image-header timestamps unchanged |
| Exported six patches | Applied with `git am --whitespace=error` from the recorded upstream base; final tree matches exactly |
| Source mirror and commit scope | Mirror matches final branch; generated build files excluded from all six commits |
| Source whitespace | `git diff --check` passed; patch formatting checked by `git am` |

The runtime prefixes (2..5) were built with `G0_EXTENSION=yes` and
`G0_FIRMWARE_UPDATE=no`, without a companion source/image dependency. The final
prefix used Atlas defaults and automatically built its pinned G0 source
`5fdddd58c6`. The companion source/protocol were unchanged by this refactor.

Before the provisioning follow-up, the module directory's production C++/headers/make/Python code shrank from
1150 to 1101 lines, excluding tests and documentation. The refactor removes
an extra frame copy, unused counters, duplicate mode state, unused response
cases and a separate off-command path. Framework registration deduplication
replaces the extra local guard. CRC, freshness, acknowledgement, fairness and
programming/readback checks remain.

Atlas builds used vendored ARM GCC 11.3. Java tools ran with JDK 25 producing
Java 21 output in a task-specific Gradle cache. Leak detection was disabled for
this ptrace environment; AddressSanitizer and UndefinedBehaviorSanitizer remained
enabled. Builds ran outside the user's checkout. Existing branches, uncommitted
VS Code settings and the untracked companion directory were preserved.

The factory-provisioning follow-up was validated on 2026-11-14:

| Check | Result |
| --- | --- |
| Module tests | All four CTest groups passed with AddressSanitizer and UndefinedBehaviorSanitizer; Python image tests passed |
| Option provisioning | Factory and already configured options, preserved non-default fields, invalid complements, RDP, nBOOT1, NRST mode, BOOT_LOCK, unstable reads and failed readback covered |
| ROM reconnect | Already synchronized ROM, wrong device ID, missing final option ACK and bounded connection timeouts covered |
| Production updater | Factory first flash without any NRST pulse, matching app, older app, invalid options and absent device covered with HAL mocks |
| Actual G0 slave and identity regressions | Passed; companion source and pin unchanged |
| Clean Atlas build | Bootloader and application passed using the normal board script |
| Compiled startup GPIO settings | Both OpenBLT and the main application set GPIOB PUPDR to `0x9aaaaaaa`, keeping PB14/NRST an input with pull-up |
| Bundled image | All 12,060 bytes occur exactly once in packaged `fome.bin`; readonly ELF symbol at `0x080d60d0`; identity `0x722cede0` |
| Exported seven patches | Applied from the recorded upstream base with `git am --whitespace=error`; final tree matches the provisioning commit |
| Module mirror | All 33 files match the final fome-fw module |

The full 770-test native suite result above belongs to the original compact
stack; it was not rerun for this hardware updater follow-up. Generated files
were excluded from the follow-up commit as well.

Hardware timing, electrical levels, BOOT option bytes and physical/interrupted
ROM programming were not tested. Nothing was flashed. SENT remains unsupported.

Upload follow-up validation on 2026-11-15:

- All four standalone CTest groups pass with AddressSanitizer and UndefinedBehaviorSanitizer.
- Actual updater tests require the post-Go reset, verify no premature factory reset,
  count a 20 us polled delay per ROM byte and a 1 ms scheduler sleep per 32 bytes,
  exercise upload LED activity, and reject a failed Go without resetting the G0.
- The Atlas board script built and packaged both OpenBLT and the application.
- All nine exported patches apply with `git am --whitespace=error` from the recorded
  upstream base and reproduce the final source tree. The module mirror matches.

The observed minute-long transfer is explained by 20 us RTOS sleeps rounding up
at Atlas's 1 kHz tick frequency. The new byte spacing removes that delay without
reducing ST's required minimum gap. Actual transfer duration and concurrent-thread
behavior still require a bench retest. The normal LED callback starts after
hardware initialization; upload supplies its own running-LED activity. Startup
still waits for verified remote I/O. No hardware was flashed during these checks.

SPI5 chip-select follow-up validated on 2026-11-15:

- A temporary C++ assert harness executed the actual CS preparation block with
  empty configuration, another SPI bus, multiple valid CS pins, an unassigned pin
  and an expansion GPIO pin. It checked that each output latch was HIGH before
  enabling its driver and that only the selected local SPI5 CS pins were touched.
- All four standalone sanitizer-enabled CTest groups passed.
- The Atlas bootloader/application board build and packaging passed.
- The tenth patch applied with `git am --whitespace=error` after the previously
  verified nine patches; its final tree and module mirror match the branch.

External CS pull-ups still protect the period before firmware can read the
configuration. Shared physical SPI operation has not been bench-tested.

PWM compatibility follow-up validated on 2026-11-15:

- All four FOME CTest groups and both G0 slave/identity groups pass with the
  existing sanitizer-enabled test script.
- Adapter regressions cover NaN initialization, zero/sub-1 Hz/NaN pause and resume,
  inverted inactive levels, invalid frequency rejection, and retained duty.
- Global disable is tested without a subsequent controller callback, alongside
  updates and new PWM startup while disabled, restore, static GPIO independence,
  and deinitialization while disabled. The tests inspect actual queued wire
  requests and acknowledge the commands.
- The Atlas board script builds and packages OpenBLT and the main application.
- The eleventh patch applies with `git am --whitespace=error` after the previously
  verified ten patches; the final tree and module mirror match the PWM branch.

The G0 image/protocol are unchanged. PWM disable follows the asynchronous SPI
schedule. Physical PWM timing, inactive polarity and watchdog behavior still
require bench verification; no hardware was flashed.

BOOT0/SWCLK release follow-up validated on 2026-11-18:

- All four FOME CTest groups and both G0 slave/identity groups passed through
  the sanitizer-enabled test script.
- Updater regressions require BOOT0 to remain driven during reset selection
  and all SPI exchanges. Factory upload, matching app, older app, invalid options,
  failed Go and absent-device paths all require it to finish as input/pull-down
  after SPI is stopped/released. NRST remains driven HIGH as before.
- A fresh isolated Atlas bootloader/application build and image packaging passed.
- All twelve exported patches applied from the recorded upstream base using
  `git am --whitespace=error`; their final tree and the module mirror match.

The earlier live diagnosis confirmed that releasing H7 PB15 allows G0 SWD access
while H7 runs. This newly built firmware was not flashed. The isolated build uses
the published protocol-9 stack; the user's local protocol-10 and debug edits are
preserved when applying the source fix to the working checkout. No option bytes
or NRST behavior were changed.

Build-system follow-ups validated on 2026-11-18, based on the updater's upstream
merge `79b594285f`:

- `test_enum_generation.py` passes for firmware and bootloader contexts: cached
  no-op, tracked enum header replaced after generation, enum input changed, enum
  header missing, stage output missing, and missing stage manifests.
- `test_checksum_tools.py` passes tool-selection, quoted filename, selection-failure
  propagation and existing custom TRGT override checks. Host-tool sentinels ensure
  packaging uses ARM tools rather than objcopy/objdump found on PATH.
- The ordinary Atlas script builds and packages both OpenBLT and the main image
  without wrappers. A second normal build recovers after the GPIO-name header is
  restored from Git while both generation stamps remain present.
- The two separate build-fix patches apply with `git am --whitespace=error` from
  `79b594285f` and reproduce the final branch tree. Generated files are excluded
  from both commits. The original twelve patches are unchanged.

The user's untracked `firmware/tests/test_generated_make.py` was left intact.
No hardware was flashed; the full native engine-control suite was not rerun for
these build-rule and packaging changes.

Selected audit follow-ups validated on 2026-11-19:

- All four host and both slave/version CTest groups pass with sanitizers; the
  actual FOME module tests pass as well. Existing suites were extended for the
  source-only build dependency, pinned/clean ChibiOS rejection, reset mode and
  unsupported update command.
- A native build with a temporary companion header using protocol 1234 compiles
  every translation unit with an assertion for that version; all four groups
  pass without changing the host protocol wrapper.
- After cleaning both bootloader and application build directories, the normal
  Atlas board script builds and packages the entire firmware successfully.
- The 12,016-byte G0 image occurs exactly once in packaged `fome.bin`; the bundled
  identity matches `0x26fa15cb`. Offline GDB reports protocol 9 in both ELF files.
- All four patches in [audit_fixes](audit_fixes/README.md) apply from `de73644150`
  using `git am --whitespace=error` and reproduce the final branch tree. The source
  mirror matches FOME exactly. Generated and unrelated user files are excluded.

The full native engine-control suite was not rerun. No hardware was flashed;
NRST electrical behavior still needs a bench check. SPI timeout/recovery was
explicitly skipped, so its existing failure-mode limitations remain.

The six open G0 PR branches were refreshed on 2026-11-19:

- Protocol, transport, ADC and digital-input branches each pass their standalone
  sanitizer suite; the output branch passes both groups, and the updater passes
  all four groups.
- All six branches use the companion protocol header pinned at `689042c9be`.
  Source initialization is carried by the transport PR, without bundling firmware
  in runtime-only branches. The updater retains the clean/pinned ChibiOS guard,
  open-drain reset and removed application update mode.
- The updater's production files match the already validated Atlas build; only
  its module README differs. The mirror was updated to that README. A second ARM
  build and hardware flashing were not needed for these branch-history updates.
- Every remote PR branch advances from its original published tip and contains
  its updated predecessor. Updates were pushed atomically without force.

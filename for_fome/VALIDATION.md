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

Validated the compact stack on 2026-11-14, based on `upstream/master`
`e628773bd0`. Its six commits and branch names are recorded in
[series.json](series.json); the final updater tip is `0cb6472e53`.

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

The module directory's production C++/headers/make/Python code shrank from
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

Hardware timing, electrical levels, BOOT option bytes and physical/interrupted
ROM programming were not tested. Nothing was flashed. SENT remains unsupported.

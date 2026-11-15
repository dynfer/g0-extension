Title: Build and bundle G0 firmware with verified startup updates

Atlas builds the pinned G0 source and embeds its complete firmware image and
identity in the ECU binary, so installing an ECU firmware also installs the
matching expansion firmware. Startup checks the running image identity and
programs the companion through its SPI ROM loader when needed, before remote
I/O is registered.

Factory-blank STM32G071 devices can now be provisioned without ST-Link. Startup
preserves the existing ROM session, validates device identity and option bytes,
clears only nBOOT_SEL and its inverse copy, and verifies the complete option
block before erasing main flash. Atlas keeps G0 NRST pulled up through both ECU
firmware stages. The updater verifies every programmed block, starts the G0
with ROM Go, then pulses NRST with BOOT0 low and checks the application identity
before enabling I/O. The ROM byte gap uses a cycle-counter delay instead of a
scheduler-tick sleep; a 1 ms sleep every 32 bytes keeps other threads runnable.
The running LED shows upload activity while the normal startup LED callback is
not yet running.

Builds generate the image header automatically and preserve its timestamp when
the image is unchanged. `G0_FIRMWARE_UPDATE=no` retains runtime I/O without the
image dependency; `G0_EXTENSION=no` disables the expansion.

Validation: sanitizer-enabled protocol, output, bootloader and production-updater
tests; Python image/identity tests; actual G0 slave regressions; and a clean Atlas
bootloader/application build. The packaged image contains all 12,060 companion
bytes in readonly storage. All ten exported patches apply cleanly to the
recorded upstream base and reproduce the branch tree.

Bench verification of option programming, SPI timing and interrupted updates
remains outstanding. First provisioning requires the live factory ROM session;
if it was lost, a full power cycle may be necessary. An already programmed chip
with BOOT0 disabled still needs a cooperating application or debug access to
enter ROM.

Review base: `codex/g0-v9-05-outputs`.
Head: `codex/g0-v9-06-updater`.
Factory-provisioning follow-up: `e37de65226`, based on `0cb6472e53`.

Upload/reset follow-up tip: `06ea4d7ec5` (review against `e37de65226`).

Atlas deselects configured local MAX31855 CS pins on SPI5 before G0 initialization,
including when thermocouple polling is disabled. It preloads each latch high
before enabling its output driver and leaves ownership to the later thermocouple
driver initialization. Follow-up: `4bc718d70d`.

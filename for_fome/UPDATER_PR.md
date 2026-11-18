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
bytes in readonly storage. All twelve exported patches apply cleanly to the
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

G0 PWM accepts the stopped frequency values used by tachometer/speedometer
controllers, holds the logical inactive level while paused, and resumes with
retained duty. Active PWM follows FOME's global enable flag on each SPI poll,
including when its controller makes no further calls. Setpoint changes while
disabled are retained for restoration; deinitialized channels stay off and static
GPIO remains independent. Follow-up: `03800b69d6`. Disable takes effect through
the normal SPI scheduling path.

After initialization, the updater releases BOOT0 as an input with weak pull-down
on success and failure. STM32G071 BOOT0 shares PA14/SWCLK; retaining the former
push-pull LOW blocked G0 debug attachment while H7 ran. Boot selection stays driven
during reset and ROM transfers. Follow-up: `020223048f`.

Following the upstream merge, the updater also recovers generated enum headers
replaced by Git and missing generated files left behind by another build's clean.
Packaging selects objcopy/objdump through the existing ARM toolchain Makefile,
so the ordinary board script works without host-binutils wrappers. Review these
build fixes separately against `79b594285f`; branch tip `faf5d2cc8b`.

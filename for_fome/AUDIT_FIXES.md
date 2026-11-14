The audit fixes update the slave and fome-fw together to protocol 9. Apply the
complete series or use the updated updater branch; protocol-8 hosts cannot drive
the new slave. The startup updater installs it through the existing BOOT/RESET
and ROM bootloader path.

| Finding | Change | Regression coverage |
| --- | --- | --- |
| Outputs survive communication loss | All outputs turn off after 500 ms without a valid frame | Static and PWM shutdown, timers remain stopped |
| Duty changes restart the HIGH phase | Apply duty changes at the next PWM cycle boundary | Alternating 50/51% updates every 100 ms at 1 Hz still produce LOW intervals |
| Startup output glitch | Clear the latch before changing the pin to output mode | HAL fixture starts with HIGH latches and checks every mode transition |
| Capture ISR signals an unlocked semaphore | Remove the capture worker/semaphores; lock edge callbacks correctly | ISR fixtures assert kernel-lock use |
| TIM14/16/17 lack the ICU channel pair | Use GPIO edges and a shared 1 MHz TIM6 clock | All four pins, timer rollover, signal expiry, mode changes and 1 Hz input |
| ADC can report failed or stale samples as ready | Publish locked snapshots with readiness, driver-state and age checks; install an ADC error callback | Error, queued completion after error and 250 ms expiry |
| Corrupted commands can energize another output | CRC16/CCITT-FALSE over bytes 0..33; little-endian checksum in bytes 34..35 | Every single-bit change in a 36-byte output command is rejected before actuation |
| Truncated SPI transfer loses alignment | Detect partial DMA when NSS is high, reset SPI and rearm a full frame | 35-byte transfer, asserted NSS, empty wait, completion and rearm |
| Different builds share application version 8 | Generate a 32-bit build fingerprint separately from protocol version 9; compare both before skipping programming | Source/compiler/options/dependency changes, stable headers and arbitrary image identities |
| Lua frequency changes never reach G0 | Forward `SimplePwm::setFrequency` to the hardware adapter and queue the retained duty/inversion | Actual G0 adapter, invalid frequency, inversion and deinitialization |
| SENT decoder accepts invalid frames | Reject SENT mode and reads until a validated decoder exists | Mode and command rejection |
| Tests exercise only host models | Compile the actual slave sources and production output adapter with small HAL fixtures | AddressSanitizer and UndefinedBehaviorSanitizer |
| Unused capture/SENT queues consume RAM and drop entries | Delete the queues, batch APIs, decoder and worker | Remaining production callers compile in the firmware builds |

The firmware identity covers application sources/headers, board/configuration
files, Makefile, generator, protocol, pinned ChibiOS revision, compiler version
and build options. Updating the pinned G0 revision is sufficient for the normal
Atlas build to regenerate and bundle its image. No manually incremented release
number or prepared binary header is needed.

GPIO capture measures interrupt arrival times, so input rate and timing accuracy
still need bench verification. SENT is deliberately unavailable. Nothing has
been flashed; ROM boot behavior, interrupted programming, output electrical
levels and SPI recovery timing still require the real Atlas/expansion hardware.

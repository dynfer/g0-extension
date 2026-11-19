The extension is an STM32G071 companion MCU running its own ChibiOS firmware.
It is an asynchronous I/O coprocessor for fome-fw, with a fixed-size SPI slave
interface. The original comparison below used extension revision `19f0c4c` and fome-fw
`upstream/master` at `b3ae7a9aa0`. The current audit fixes are described in
[AUDIT_FIXES.md](AUDIT_FIXES.md) and require protocol 9 on both MCUs. The compact
six-branch stack targets `upstream/master` at `e628773bd0`; its protocol, safety
and updater fixes are included in their owning module commits.

| Source | Behavior |
| --- | --- |
| `analog.cpp` | Continuous ADC, fourfold hardware oversampling and software averaging; twelve pin voltages in millivolts |
| `inputs.cpp` | Four digital/frequency inputs using GPIO edges and a shared 1 MHz timer; SENT rejected |
| `outputs.cpp` | Four independent software PWM outputs, static output at frequency zero, 1..500 Hz PWM, duty 0..10000 |
| `spi_app_protocol.cpp` | 36-byte full-duplex SPI frames; the next exchange returns the response just prepared |
| `for_fome/g0_spi_protocol.h` | Packed request/response layouts and payload sizes |
| `main.cpp` | Starts sampling, capture, outputs, and SPI; displays update-request status through LEDs |

The branches contain different layers of this work:

| fome-fw branch | Observed implementation |
| --- | --- |
| `g0-extension` (`6d2c810bb9`) | Atlas startup firmware loading and extension image submodule |
| `wip` (`daaca9a4d3`) | Older integration work alongside ADC provider changes |
| `wip2` (`939c60ad21`) | Separate loader files, an ADC-owned private polling thread, a pending output manager, virtual pins and Atlas wiring |
| `new_g0_extension` (`6c3f9b00ad`) | A larger `g0_io.cpp` based on the shared background SPI worker, plus loader and output support |
| `origin/spi_thread` (`8c2384c07c`) | Background SPI infrastructure already present in the selected upstream base |

The new code retains the 36-byte frame size and board mapping while using
fome-fw's current ADC and GPIO extension interfaces. Runtime communication,
analog state, digital state, output scheduling, and ROM programming have separate
source files. The updater has its own build option; the final Atlas build enables it by default
and automatically builds and bundles a pinned companion firmware image. Board pins and SPI register settings remain in the Atlas board file.

Findings that affect the implementation:

1. A reply belongs to the preceding exchange. Matching it to the newly transmitted
   request can acknowledge the wrong output or interpret another reply as ADC data.
   The new link stores the pending request, rejects unsolicited startup data, and
   validates command, payload length, status/result, and indexed channel.
2. The `wip2` output acknowledgement path only checks `result`, and its desired
   output state has no synchronization between producers and the worker. The new
   adapter uses critical sections and only clears a queued state after a matching
   applied frequency/duty acknowledgement. Changes made while an ACK is in flight
   remain pending. Output channels and feature modules are polled fairly.
3. The old ADC path can retain samples indefinitely and accepts partial channel
   counts. The new provider requires all twelve valid samples and expires data
   after 250 ms. fome-fw's ADC subscription code must keep NaNs out of its IIR
   filter, invalidate unavailable sensors, and preload the filter after recovery.
4. The shared SPI bus must be configured for each transfer under the existing
   worker's bus lock. DMA buffers are in noncached SRAM. No application callback
   performs synchronous SPI or holds a system lock across an exchange.
5. Reusing protected GPIO enum values makes unrelated drivers ambiguous. The new
   pins occupy unused IDs 215..222. Native GPIO chip registration supplies pin
   ownership and deinitialization; remote PWM sends setpoints to the G0 instead
   of trying to send each edge through SPI.
6. The unused application `cmdEnterUpdate` mode was removed; its command number
   remains reserved and receives an unsupported-command response. The updater
   uses the dedicated BOOT and RESET lines before registering runtime I/O, checks
   image bounds/vectors, verifies every programmed block by reading it back, and
   checks the application version after reset. It never downloads a moving branch
   during an ECU build.

Remaining implementation limits:

- SENT mode and reads are rejected until a checksum-validated decoder is available.
- Frequency capture timestamps GPIO interrupts. Bench testing must establish its
  useful input rate and timing accuracy under ADC/SPI load. Period and HIGH-time
  fields saturate at 65535 microseconds; the full internal period still supports
  slow inputs. The compact all-input response saturates frequency at 65535 Hz.
  Registering frequency sensors in fome-fw remains separate from this series.
- PWM uses virtual timers at a 100 kHz system tick. At 500 Hz there are only 200
  ticks per period, so wire duty resolution does not equal edge resolution.
- ADC conversion assumes a fixed 3300 mV reference; board calibration still applies.
- SPI CRC16 detects corrupted frames; there is no transaction sequence number.
  Partial transfers are aborted after NSS returns high, with the slave checking
  at 1 ms intervals. The host leaves 10 ms between runtime transfers.
- Real ROM boot support depends on the G071 BOOT option bytes and board wiring.

The audit follow-up adds the missing output watchdog, preserves PWM cycles across
updates, rejects corrupted commands before applying them, invalidates failed or
stale ADC snapshots and separates firmware identity from protocol compatibility.
The build generates the identity automatically, so source changes no longer share
a manually fixed application version.

The byte-level loader was checked against
[ST AN4286](https://www.st.com/resource/en/application_note/an4286-how-to-use-spi-protocol-in-bootloader-on-stm32-mcus-stmicroelectronics.pdf).
Host tests emulate its exchanges and failures; physical SPI, capture timing,
electrical output behavior, option bytes, and interrupted programming require
hardware validation.

# ArduinoX360-tinyusb-R4

Xbox 360 (XInput) gamepad emulation for **Arduino Uno R4 Minima** and **Arduino
Nano R4** only, on the [UnoR4-tinyusb](https://github.com/controllercustom/UnoR4-tinyusb)
stack (UnoR4 TinyUSB Boards package, vendored TinyUSB master). No Adafruit TinyUSB,
no Arduino-TinyUSB — this library is R4-only by design.

- Public API matches the sibling library `ArduinoX360-tinyusb` (same `Button`
  enum, triggers, sticks, hat, `onRumble`/`onLed`) except for one deliberate
  omission: `setPollInterval`. Reports here are **loop-pumped** — the sketch
  calls `send()`/`update()` (and `pollRumble()` for host rumble/LED packets) in
  `loop()`, and there is no internal poll timer for such a method to steer.
- Transport: TinyUSB vendor interrupt pair (IN `0x81` / OUT `0x01`) + vendor
  control requests (GET_REPORT / capabilities). Enumerates `045E:028E`.
- **Every example builds in the plain Arduino IDE 2.x**: each sketch folder
  carries its own `tusb_config.h` with `CFG_TUD_VENDOR_EP_INT_IN/OUT 1`; the
  package's `-iquote{build.source.path}` makes it live for every TU, core
  included. No custom compiler flags, anywhere.
- Descriptors are library-owned (strong `tud_descriptor_*_cb` override the core's
  weak ones), so each sketch stays one `.ino` + `tusb_config.h`.

## Install (Arduino IDE 2.x)

1. **Board package** — add UnoR4-tinyusb's board manager URL
   (`https://raw.githubusercontent.com/controllercustom/UnoR4-tinyusb/refs/heads/main/package_unor4tinyusb_index.json`),
   then Boards Manager → **UnoR4 TinyUSB Boards**.
2. **This library** — install it with the IDE's ZIP-library feature:
   **Sketch → Include Library → Add .ZIP Library…**, choosing a `.zip` archive
   whose **root is the library folder** (`ArduinoX360-tinyusb-R4/` containing
   `library.properties`, `src/`, `examples/`, `tests/`). The feature installs
   into the `libraries` folder of your sketchbook, so it behaves the same on
   Windows, Linux and macOS and follows a non-default sketchbook location
   (Preferences → Sketchbook location) — no paths to type.
   If your archive has the library one level down (typical of a source
   archive such as `…/archive/refs/heads/main.zip`, whose root is
   `ArduinoX360-tinyusb-R4-main/`), re-pack it first: the IDE rejects or
   mis-names archives whose root is not the library folder.
   `depends=UnoR4-tinyusb` needs the stack library installed the same way,
   from the UnoR4-tinyusb repository.
3. **Pick the board**: UnoR4 TinyUSB Boards → **UNO R4 Minima (TinyUSB)** or
   **Nano R4 (TinyUSB)**.
4. Open an example (File → Examples → ArduinoX360-tinyusb-R4) and build. If the
   examples are not listed yet, restart the IDE — ZIP-installed libraries
   expose their `examples/` folder only after a restart.

Picking the wrong board fails at compile time instead of half-building: the header
`#error`s on non-Renesas cores (ESP32, SAMD, RP2040 …), and `ArduinoX360_R4.cpp`
`#error`s when the vendor interrupt-EP pair is not compiled in — which is what
catches the stock `arduino:renesas_uno` core, since that core passes the same
`-DARDUINO_ARCH_RENESAS` but has no `-iquote{build.source.path}`, so the sketch-local
config reaches no TU.

> **Before you upload**: double-tap the board's **RESET** button (press and release
> it twice, quickly) so the board enters its bootloader and offers the DFU port the
> IDE uploads to — the "L" LED pulses while it waits. TinyUSB owns the board's USB
> connector, so the running sketch does not leave the stock serial port the IDE
> would otherwise use to reach the bootloader, and an upload that is not preceded by
> the double tap fails with "Cannot open DFU device". One more press returns the board
> to application mode. If you want the stock behaviour back — automatic uploads and
> the IDE Serial Monitor — select the regular **Arduino UNO R4 Minima** / **Nano R4**
> board and upload any sketch: that restores the stock USB path. These sketches do
> not build against that core (guard above), so the switch is a way back to a normal
> R4, not a way to run this library.

## Console wiring (all sketches)

The R4's own USB connector **is** the TinyUSB gamepad port — the IDE Serial
Monitor shows nothing. The console is `Serial1`: an external USB-UART bridge
(PL2303 here) on the board's **Rx/Tx/GND** pins, at **230400** baud (PL2303 max;
sketches override `UNOR4_TINYUSB_BAUD` so `Serial1.print` never throttles the
command loop). Adapter wiring: adapter RX→pin 1, adapter TX→pin 0, GND→GND.

## Examples

* `BasicButtons` — press/release A in a loop.
* `BasicGamepad` — UART command parser (`BTN_A=1`, `LX=10000`, `TRIG_L=200`,
  `RELEASE`, `DUMP`, …).
* `FullController` — physical gamepad: buttons on D2–D9, d-pad on D10–D13,
  sticks/triggers from the 14-bit ADC (A1/A2/A3/A4/A5/A0,
  `analogReadResolution(14)`). D0/D1 stay free for the console.
* `JoystickTest` — stick/trigger sweeps.
* `RumbleFeedback` — `onRumble`/`onLed` telemetry as a host sends FF_RUMBLE/LED.
* `AutoCycle` — deterministic 9-phase cycle for pcap verification
  (`scripts/analyze_xinput_pcap.py`).

Everything below this point is for maintainers — how this repo is built, benched
and verified on the machine it was developed on. IDE users can stop here: install
the stack, pick the board, open an example, double-tap RESET, upload.

## Tests (on-board suites, same console)

`TestBasicFunctionality` (API state machine), `TestOutputCallbacks` (rumble/LED
round-trips, awaits OUT packets), `LatencyBenchmark` (timestamp emitter +
`scripts/run_tests.py` evdev correlation).

## Build (CLI gate = IDE simulation)

```bash
./scripts/build.sh                     # all 9 sketches x {minima, nanor4}, no flags
./scripts/build.sh BasicGamepad r4     # one sketch, one board
```

`build.sh` passes **no** `--build-property` flags — byte-identical to an IDE
build — and proves the sketch config reached the core TU (`vendor_device.c.o`
defines `tud_vendor_n_int_write`) and the full backend was selected
(`ArduinoX360_R4.cpp.o` references it). There is no stub backend any more: a
build that loses the pair fails at compile time with an actionable `#error`.

Upload (stock DFU path, same as the IDE):

```bash
arduino-cli upload --fqbn unor4tinyusb:renesas_uno:minima --input-dir build/X_r4
```

## Bench (this machine: Minima + J-Link PLUS SWD + PL2303)

```bash
./scripts/bench.sh BasicGamepad r4 upload    # stock dfu-util path (board in DFU: 2341:0369)
./scripts/bench.sh BasicGamepad r4 flash     # J-Link/SWD reflash (device-mode sketches kill DFU)
./scripts/bench.sh BasicGamepad r4 send "BTN_A=1" "DUMP" "RELEASE"
./scripts/bench.sh BasicGamepad r4 jlink     # SWD snapshot of the report buffer
```

After the first `upload`, the running sketch owns the board's USB connector in
device mode, so there is no DFU any more — reflashes go over SWD with the J-Link
PLUS (`si SWD` / `device R7FA4M1AB` / `speed 4000`). `jlink` halts the target,
which makes the PL2303 drop bytes: snapshot after feeding, never during, and
press RESET if the target ends up held in reset.

## Hardware results (bench: Uno R4 Minima + J-Link PLUS SWD + PL2303, kernel 7.0.0-38)

**Build gate**: 18/18 — every sketch × {`minima`, `nanor4`} compiles with **zero**
custom flags, and each build proves the sketch-local `tusb_config.h` reached the
core's vendored vendor TU (`vendor_device.c.o` defines `T tud_vendor_n_int_write`)
and that the full backend was linked (`ArduinoX360_R4.cpp.o` references it).
There is no stub backend: a build that loses the pair fails at compile time.

| sketch | flash (minima / nanor4) | RAM |
|---|---|---|
| BasicButtons | 40672 / 40716 (15%) | 6732 |
| BasicGamepad | 43488 / 43532 (16%) | 7144 |
| FullController | 41180 / 41224 (15%) | 6732 |
| JoystickTest | 40912 / 40956 (15%) | 6732 |
| RumbleFeedback | 41276 / 41320 (15%) | 6732 |
| AutoCycle | 52988 / 53028 (20%) | 7100 |
| TestBasicFunctionality | 43216 / 43260 (16%) | 6736 |
| TestOutputCallbacks | 53180 / 53228 (20%) | 7124 |
| LatencyBenchmark | 42564 / 42608 (16%) | 6816 |

**Upload + enumeration**: stock `dfu-util` path (`2341:0369`, 43368 B). Kernel
answers `idVendor=045e idProduct=028e`, `Product: XInput Controller`,
`Manufacturer: Microsoft`, `SerialNumber: 123456`, and binds
`input: Microsoft X-Box 360 pad` → `/dev/input/event9` + `/dev/input/js0`,
driver `xpad`.

**IN reports**: byte-exact. `DUMP` after `BTN_A=1 LX=10000 LY=-12000 TRIG_L=200`
reads `RPT=001400100100102720D1…` → `wButtons=0x1000` (A), `LX=0x2710=10000`,
`LY=0xD120=-12000`, `bLeftTrigger=1`. Host side agrees on `event9`:
`1 304 1` (BTN_A down), `3 0 10000` (ABS_X), `3 1 11999` (ABS_Y — xpad negates
and rescales Y), trigger on `ABS_Z`.

**OUT endpoint**: live, and the callback path is ISR-safe. xpad's init packets
arrive on the vendor interrupt OUT EP; the ISR only queues them, and
`pollRumble()` (called from `loop()`) fires the callbacks — printing directly
from the ISR deadlocks the R4 console, which the bench proved: before the fix,
`TestOutputCallbacks` hung with `PC` inside `UART::write` and `IPSR = 010`.
Observed after the fix: `CB_LED:2`, `CB_LED:6` and, in `RumbleFeedback`,
`LED change requested, index: 2 / 6`. Rumble motors stay `0,0` because this
kernel's xpad has no force-feedback path (`xpad_rumble` and the js FF ioctls are
gone in kernel 7.0), so no host here can request nonzero motor values.

**On-board suites** (console at 230400, results read straight off the wire):

* `TestBasicFunctionality` — `PASS: 57  FAIL: 0  ALL TESTS PASSED`. In the latest
  run the two counters were also read straight out of RAM over SWD
  (`_ZL4pass` = 0x39, `_ZL4fail` = 0), which does not depend on the console.
* `TestOutputCallbacks` — `PRE-USB RESULTS: PASS=7 FAIL=0`, then the per-iteration
  checks keep `fail=0` while the OUT callbacks fire (`led=2` in the running
  SUMMARY, `CB_LED:2` / `CB_LED:6` from the callbacks).

**Latency** (`LatencyBenchmark` + `scripts/run_tests.py`, 666 UART records vs
363 evdev events, 664 correlated; microseconds):

| event | count | min | median | p95 | max |
|---|---|---|---|---|---|
| button_press | 64 | 153 | 1253 | 9338 | 11104 |
| button_release | 64 | 148 | 1279 | 12125 | 13251 |
| dpad | 36 | 11 | 657 | 1996 | 2001 |
| left_stick | 200 | 0 | 539 | 1488 | 2152 |
| left_trigger | 100 | 0 | 1226 | 11911 | 13010 |
| right_stick | 100 | 6 | 623 | 1482 | 1998 |
| right_trigger | 100 | 103 | 1317 | 12845 | 13941 |
| **all** | 664 | 0 | **844** | 9531 | 13941 |

Microseconds, from the device's `micros()` stamp to the xpad evdev timestamp.

**Methodology caveat** (`scripts/run_tests.py`, `correlate()`): each UART record
is paired with the *nearest* evdev event within 100 ms using an **absolute**
difference, and nothing checks that the evdev code corresponds to the record's
type. So this is a nearest-match distance, not a directional latency: a device
stamp that lands *after* the host event counts as positive, and a stick record
can be paired with a button event. The sub-millisecond medians are the credible
part of the table; the multi-ms p95s are consistent with xpad's poll interval
but this harness does not prove that attribution. Tighten it by filtering on the
event code per record type and by keeping the sign of the difference.

**Console baud** (PL2303, 200-line burst, byte-for-byte reply check):

| baud | clean replies | rate |
|---|---|---|
| 230400 | 200/200 | 2779 cmd/s (wire-bound: 400 B ≈ 17.4 ms) |
| 460800 | 194/200 (6 corrupted) | 4168 cmd/s |
| 921600 | 146/200 (54 corrupted) | 3.6 cmd/s |

230400 is the ceiling that round-trips cleanly — the PL2303's documented max —
so the sketches keep `UNOR4_TINYUSB_BAUD 230400`.

**pcap stage (usbmon, run on this machine, kernel 7.0.0-38)**: usbmon is
available — `/lib/modules/7.0.0-38-generic/kernel/drivers/usb/mon/usbmon.ko.zst`
loads, and `/dev/usbmon*` nodes exist. The ones udev left behind carry a stale
major (189, from an older kernel), so `tcpdump` answers "No such device" until
they are recreated:

```bash
M=$(grep usbmon /proc/devices | awk '{print $1}')   # 511 here
sudo mknod /dev/usbmon3 c $M 3 && sudo chmod 666 /dev/usbmon3
sudo timeout 25 tcpdump -i usbmon3 -w /tmp/opencode/cap.pcap   # board on Bus 003
sudo chown me:me /tmp/opencode/cap.pcap                        # AppArmor denies tshark root-owned files
python3 ~/.opencode/skills/pcap-analysis/scripts/usbmon_decode.py /tmp/opencode/cap.pcap --in 0x81
```

The capture does prove the host-side identity: the board's `GET_DESCRIPTOR`
responses are on the wire and carry `wIDVendor 0x045e` / `wIDProduct 0x028e`
plus the vendor interface descriptor. It does **not** carry the 20-byte IN
reports: usbmon logs the board's control traffic but never a successful
interrupt-endpoint transfer — bus 3's counter in
`/sys/kernel/debug/usb/usbmon/3s` grows by ~46 events per 20 s while xpad/evdev
counts ~176 reports/s — so `scripts/analyze_xinput_pcap.py` parses 0 report
frames (`No frames parsed`). The ordering does not help: the capture was re-run
with usbmon loaded before and after the board was re-added, on Bus 001 (the
480M xHCI endpoint that also carries the PL2303) and on Bus 003, and with the
text reader (`3u`) instead of the binary one. On this machine the report stream
is therefore verified through xpad/evdev (the latency table above), and the
analyzer's phase classification is unverified.

## Layout

* `src/ArduinoX360_R4.h` — public API (unique header name, see above).
* `src/ArduinoX360.cpp` — board-agnostic state machine.
* `src/ArduinoX360_R4.cpp` — USB backend (UnoR4-tinyusb only). OUT packets are
  **queued** by the interrupt-EP callback and delivered by `pollRumble()`, so
  `onRumble`/`onLed` always run in `loop()` — printing from the USB ISR would
  deadlock the R4 console (its `Serial1.write()` waits on an SCI TX-done
  interrupt that cannot preempt the USB ISR). Every sketch here calls
  `tud_arduino_task()` and `pollRumble()` in `loop()`; your own sketches must
  call `pollRumble()` too, or queued OUT packets are never delivered and the
  callbacks stay dead.
* `src/x360r4_descriptor.h` — golden 40-byte interface block (unique name).
* `src/xinput_descriptors.cpp` — strong `tud_descriptor_*_cb` (hide the core's
  CDC identity; library-owned so sketches stay one file).
* `examples/<X>/tusb_config.h` — sketch-local config, the IDE route (see above).
* `scripts/` — build gate, bench driver, latency harness, pcap analyzer.

## Related Projects

`ArduinoX360-tinyusb` (multi-board, Adafruit TinyUSB backends) and
`ArduinoX360-tinyusb-g2` (unified Arduino-TinyUSB stack; R4 validated via
UnoR4-tinyusb). This repo freezes the R4 path and makes every sketch IDE-buildable.

## License

MIT — see `LICENSE`.

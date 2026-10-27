#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# bench.sh — upload + console driver for the hardware bench.
#
# Hardware assumed (this machine):
#   * Uno R4 Minima plugged in. DFU while the stock bootloader runs:
#     2341:0369 (minima) / 2341:0374 (nanor4). Once a TinyUSB device-mode
#     sketch runs, the board's USB connector is the GAMEPAD port: no more
#     DFU, so reflash over SWD with the J-Link instead (mode: flash).
#   * SEGGER J-Link PLUS on SWD (modes: flash, jlink).
#   * PL2303 USB-UART bridge on the board's Rx/Tx/GND pins -> /dev/ttyUSB0.
#     Adapter wiring: adapter RX->pin 1, adapter TX->pin 0, GND->GND.
#
# Usage:
#   ./scripts/bench.sh BasicGamepad r4 upload            # stock dfu-util path (first flash)
#   ./scripts/bench.sh BasicGamepad r4 flash             # J-Link reflash (after that)
#   ./scripts/bench.sh BasicGamepad r4 send "BTN_A=1" "DUMP" ...
#   ./scripts/bench.sh BasicGamepad r4 jlink             # SWD snapshot of the report buffer
#
# send mode opens the PL2303 console at the sketch baud, feeds each argument
# line, and prints every response line (READY, SENT=1, DUMP state).
#
# NOTE: jlink halts the target, which makes the PL2303 drop bytes (overrun) —
# snapshot AFTER the send sequence, never while feeding. If the target is left
# held in reset, pkill -f JLinkExe and press the board's RESET button.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

UNOR4_HOME="${UNOR4_HOME:-$HOME/Arduino/libraries/UnoR4-tinyusb}"
UART="${UART:-/dev/ttyUSB0}"
BAUD="${BAUD:-230400}"

declare -A BOARD_FQBN=(
    [r4]="unor4tinyusb:renesas_uno:minima"
    [nanor4]="unor4tinyusb:renesas_uno:nanor4"
)

sketch="${1:?usage: bench.sh <sketch> <board> <mode> [args...]}"
board="${2:?board key (r4|nanor4)}"
mode="${3:?mode (upload|flash|send|jlink)}"
shift 3

fqbn="${BOARD_FQBN[$board]:-}"
[[ -n "$fqbn" ]] || { echo "unknown board key: $board" >&2; exit 1; }
bdir="$PROJ_ROOT/build/${sketch}_${board}"
[[ -d "$bdir" ]] || { echo "no build artifact at $bdir — run scripts/build.sh $sketch $board first" >&2; exit 1; }

kind=examples
[[ -d "$PROJ_ROOT/tests/$sketch" ]] && kind=tests
sketch_dir="$PROJ_ROOT/$kind/$sketch"

case "$mode" in
    upload)
        # Stock DFU path — identical to what the Arduino IDE does (dfu-util,
        # device 2341:0369 for minima). Requires the board in DFU mode.
        arduino-cli upload --fqbn "$fqbn" --input-dir "$bdir" "$sketch_dir"
        ;;
    flash)
        # J-Link/SWD reflash: needed once the running sketch owns the USB
        # connector in device mode (no DFU any more). Incantation proven by
        # UnoR4-tinyusb's upload_r4.sh (device R7FA4M1AB, si SWD, speed 4000).
        hexf="$bdir/${sketch}.ino.hex"
        [[ -f "$hexf" ]] || { echo "no .hex at $hexf" >&2; exit 1; }
        cmdf="$(mktemp /tmp/opencode/bench_flash.XXXX.jlink)"
        logf="/tmp/opencode/bench_flash.log"
        cat > "$cmdf" <<JLINK_EOF
si SWD
device R7FA4M1AB
speed 4000
connect
r
h
loadfile $hexf
r
g
q
JLINK_EOF
        # Keep J-Link's own exit status: piping into tee|grep and ending with
        # `|| true` reported success even when loadfile or connect failed, so a
        # bench loop would happily continue on a board that was never flashed.
        rc=0
        JLinkExe -CommanderScript "$cmdf" > "$logf" 2>&1 || rc=$?
        grep -vE '^\s*$' "$logf" || true
        if [[ $rc -ne 0 ]]; then
            echo "[bench] J-Link flash FAILED (rc=$rc): see $logf" >&2
            rm -f "$cmdf"
            exit 1
        fi
        rm -f "$cmdf"
        ;;
    send)
        # Defensive: raw so that neither canonical processing nor echo mangles
        # what the feeder writes (pyserial sets its own flags too).
        stty -F "$UART" raw -echo 2>/dev/null || true
        python3 - "$UART" "$BAUD" "$@" <<'PY'
import sys, time
import serial
uart, baud, cmds = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ser = serial.Serial(uart, baud, timeout=2)

def read_until_idle(timeout=3.0, gap=0.5):
    out, line = [], b""
    start = last = time.time()
    while time.time() - start < timeout:
        b = ser.read(1)
        if b:
            last = time.time()
            if b == b"\n":
                out.append(line.decode(errors="replace"))
                line = b""
            else:
                line += b
        elif line or time.time() - last > gap:
            break
    if line:
        out.append(line.decode(errors="replace"))
    return out

print("--- initial drain ---")
for l in read_until_idle(timeout=2.0): print(l)
for c in cmds:
    ser.write((c + "\n").encode())
    print(f">>> {c}")
    for l in read_until_idle(timeout=3.0): print(l)
    time.sleep(0.05)
PY
        ;;
    jlink)
        # Symbol-driven live snapshot (no firmware changes): the last report
        # actually handed to tud_vendor_n_int_write, read over SWD while the
        # sketch runs. The static is C++-mangled (_ZL13g_x360_report), so
        # match on the name suffix.
        elf="$bdir/${sketch}.ino.elf"
        [[ -f "$elf" ]] || { echo "no elf at $elf" >&2; exit 1; }
        addr="$(nm "$elf" 2>/dev/null | awk '$3 ~ /g_x360_report$/ {print $1}' | head -1)"
        [[ -n "$addr" ]] || { echo "g_x360_report not in $elf (stub backend?)" >&2; exit 1; }
        cmdf="$(mktemp /tmp/opencode/bench_jlink.XXXX.jlink)"
        cat > "$cmdf" <<JLINK_EOF
si SWD
device R7FA4M1AB
speed 4000
connect
h
mem8 0x$addr 20
g
q
JLINK_EOF
        # Same rule as flash: J-Link's exit status decides, not the grep filter.
        logf="/tmp/opencode/bench_snapshot.log"
        rc=0
        JLinkExe -CommanderScript "$cmdf" > "$logf" 2>&1 || rc=$?
        grep -E '^[0-9A-F]{8} =' "$logf" || true
        if [[ $rc -ne 0 ]]; then
            echo "[bench] J-Link snapshot FAILED (rc=$rc): see $logf" >&2
            rm -f "$cmdf"
            exit 1
        fi
        rm -f "$cmdf"
        ;;
    *)
        echo "unknown mode: $mode" >&2; exit 1 ;;
esac

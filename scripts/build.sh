#!/usr/bin/env bash
# build.sh — compile ArduinoX360-tinyusb-R4 sketches with arduino-cli,
# EXACTLY as the Arduino IDE 2.x builds them: no custom compiler flags.
#
# The vendor interrupt-EP pair (IN 0x81 / OUT 0x01) comes from the
# sketch-local tusb_config.h next to every sketch: the UnoR4 TinyUSB
# board package adds -iquote{build.source.path} to its platform.txt,
# searched before -I, so that copy wins for every TU — the core's
# vendored vendor_device.c included. The gate PROVES it: after each
# build it requires tud_vendor_n_int_write in vendor_device.c.o (the
# config reached the core TU) and referenced by ArduinoX360_R4.cpp.o
# (full backend selected, not the stub).
#
# Usage:
#   ./scripts/build.sh                     # all sketches x both boards (full gate)
#   ./scripts/build.sh --full              # same as default
#   ./scripts/build.sh BasicGamepad r4     # one sketch, one board
#
# Board keys:
#   r4       UnoR4 TinyUSB Boards -> UNO R4 Minima  (unor4tinyusb:renesas_uno:minima)
#   nanor4   UnoR4 TinyUSB Boards -> Nano R4        (unor4tinyusb:renesas_uno:nanor4)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

UNOR4_HOME="${UNOR4_HOME:-$HOME/Arduino/libraries/UnoR4-tinyusb}"

declare -A BOARD_FQBN=(
    [r4]="unor4tinyusb:renesas_uno:minima"
    [nanor4]="unor4tinyusb:renesas_uno:nanor4"
)

ALL_SKETCHES=(
    BasicButtons
    BasicGamepad
    FullController
    JoystickTest
    RumbleFeedback
    AutoCycle
    TestBasicFunctionality
    TestOutputCallbacks
    LatencyBenchmark
)

BOARDS_ALL=(r4 nanor4)

# ─── Helpers ───
ensure_core() {
    arduino-cli core list 2>&1 | grep -q "unor4tinyusb:renesas_uno" \
        || { echo "[build] ERROR: UnoR4 TinyUSB Boards package not installed." >&2
             echo "[build] Add the board manager URL (see UnoR4-tinyusb README) or run:" >&2
             echo "[build]   arduino-cli board-index update && arduino-cli core install unor4tinyusb:renesas_uno" >&2
             exit 1; }
}

# The stack library must be checked out on disk (this repo's depends= line
# names it; the IDE installs it under ~/Arduino/libraries/UnoR4-tinyusb).
check_stack() {
    [[ -d "$UNOR4_HOME" ]] || { echo "[build] ERROR: UnoR4-tinyusb checkout not found at $UNOR4_HOME" >&2; exit 1; }
}

# Header collisions: ArduinoX360-tinyusb and ArduinoX360-tinyusb-g2 both ship
# <ArduinoX360.h>, and the user's libraries dir is searched before --libraries
# — so if this project used the same header name, installing it beside either
# sibling would silently resolve to the sibling's copy (symptom: ArduinoX360_g2.cpp.o
# in the build). This repo avoids the whole class of problem by using UNIQUE
# header names: <ArduinoX360_R4.h> and "x360r4_descriptor.h" are not provided
# by any sibling, so include resolution cannot be hijacked.
#
# --library (singular) is still passed: it outranks the user's libraries dir
# and pins the build to exactly UnoR4-tinyusb + this library, so the gate
# measures this repo rather than whatever else is installed.

build_sketch() {
    local kind="$1"    # examples | tests
    local sketch="$2"
    local board="$3"
    local fqbn="${BOARD_FQBN[$board]}"
    local sketch_dir="$PROJ_ROOT/$kind/$sketch"

    if [[ ! -d "$sketch_dir" ]]; then
        echo "[build] SKIP: $sketch (directory not found)"
        return 0
    fi
    if [[ ! -f "$sketch_dir/tusb_config.h" ]]; then
        echo "[build] ERROR: $sketch has no sketch-local tusb_config.h (IDE 2.x rule)." >&2
        return 1
    fi

    local bdir="$PROJ_ROOT/build/${sketch}_${board}"
    rm -rf "$bdir"

    echo "==================== building $sketch ($board) ===================="
    # NO --build-property here: this is the IDE build, byte for byte.
    if ! arduino-cli compile \
        --fqbn "$fqbn" \
        --library "$UNOR4_HOME" \
        --library "$PROJ_ROOT" \
        --build-path "$bdir" \
        "$sketch_dir"; then
        echo "[build] FAILED: $sketch ($board)" >&2
        return 1
    fi

    # Prove the sketch-local config reached the core's vendor TU and that
    # the full backend (not the stub) was selected.
    #
    # NOTE: symbol output is captured into variables, never piped into
    # grep -q. With `set -o pipefail`, grep -q exiting on its first match
    # closes the pipe, nm dies of SIGPIPE, and the pipeline reports failure
    # at random — which looked exactly like "stub backend selected".
    local vendor_o backend_o vsym="" bsym="" estub="" ok=1
    vendor_o="$(find "$bdir" -name "vendor_device.c.o" 2>/dev/null | head -1)"
    backend_o="$(find "$bdir" -name "ArduinoX360_R4.cpp.o" 2>/dev/null | head -1)"
    [[ -n "$vendor_o" ]] && vsym="$(nm "$vendor_o" 2>/dev/null)" || true
    [[ -n "$backend_o" ]] && bsym="$(nm "$backend_o" 2>/dev/null)" || true
    estub="$(strings "$bdir"/*.elf 2>/dev/null)" || true
    if [[ -z "$vendor_o" || "$vsym" != *"T tud_vendor_n_int_write"* ]]; then
        echo "[build] FAIL: vendor_device.c.o lacks tud_vendor_n_int_write — sketch config did not reach the core TU" >&2
        ok=0
    fi
    if [[ -d "$bdir/libraries/ArduinoX360-tinyusb-g2" ]]; then
        echo "[build] FAIL: sibling library ArduinoX360-tinyusb-g2 was compiled — include resolution not isolated" >&2
        ok=0
    fi
    if [[ -z "$backend_o" || "$bsym" != *"U tud_vendor_n_int_write"* ]]; then
        echo "[build] FAIL: ArduinoX360_R4.cpp.o does not reference tud_vendor_n_int_write — stub backend selected" >&2
        ok=0
    fi
    if [[ "$estub" == *"backend needs"* ]]; then
        echo "[build] FAIL: stub message present in ELF" >&2
        ok=0
    fi
    # The stub backend was replaced by a hard #error, so the last two checks
    # above are belt-and-braces: a build that loses the vendor interrupt-EP
    # pair should not reach this point at all.
    [[ $ok -eq 1 ]] || return 1
    return 0
}

# ─── Parse args ───
if [[ "${1:-}" == "--full" ]]; then shift; fi

SKETCH_FILTER="${1:-}"
BOARD_FILTER="${2:-}"
SKETCH_FILTER="${SKETCH_FILTER#examples/}"
SKETCH_FILTER="${SKETCH_FILTER#tests/}"

ensure_core
check_stack

if [[ -n "$SKETCH_FILTER" ]]; then
    SKETCH_LIST=("$SKETCH_FILTER")
else
    SKETCH_LIST=("${ALL_SKETCHES[@]}")
fi

rc=0
for sketch in "${SKETCH_LIST[@]}"; do
    kind=examples
    if [[ -d "$PROJ_ROOT/tests/$sketch" ]]; then kind=tests; fi
    for board in "${BOARDS_ALL[@]}"; do
        if [[ -n "$BOARD_FILTER" && "$board" != "$BOARD_FILTER" ]]; then continue; fi
        if ! build_sketch "$kind" "$sketch" "$board"; then
            rc=1
        fi
    done
done

if [[ $rc -eq 0 ]]; then
    echo "[build] all done."
else
    echo "[build] FAILED (see errors above)." >&2
fi
exit $rc

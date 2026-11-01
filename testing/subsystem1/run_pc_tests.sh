#!/bin/sh
# Builds and runs every PC test of Subsystem 1. No Pico needed.
# Run from the repo root:   sh testing/subsystem1/run_pc_tests.sh
# Needs gcc. Prints one result line per test program and exits 1 if any failed.
T=testing/subsystem1
A=app_program/subsystem1
OUT=${TMPDIR:-/tmp}/s1_pc_tests
mkdir -p "$OUT"
MODULES="$A/trans_queue.c $A/com_state.c $A/status_led.c $A/telemetry.c"
FAILED=0
for t in test_data_layout test_trans_queue test_com_state test_status_led test_telemetry \
         test_scenario test_comms_cmd test_comms_frame test_comms_fresh test_comms_task test_comms_threads; do
    if ! gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -pthread -DCOMMS_HOST_TEST \
         -I$T/stubs -I$A $T/$t.c $MODULES -o "$OUT/$t"; then
        echo "BUILD FAILED: $t"; FAILED=1; continue
    fi
    if "$OUT/$t" > "$OUT/$t.txt" 2>&1; then
        echo "PASS  $t   $(grep -ciE 'pass' "$OUT/$t.txt") PASS lines"
    else
        echo "FAIL  $t   (see $OUT/$t.txt)"; FAILED=1
    fi
done
exit $FAILED
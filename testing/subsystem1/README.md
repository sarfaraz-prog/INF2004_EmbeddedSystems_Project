# Subsystem 1 tests

| What | Where | Needs |
|---|---|---|
| PC tests (logic only, no Pico) | `test_*.c` in this folder. Run `sh testing/subsystem1/run_pc_tests.sh` from the repo root | gcc |
| Fakes for the platform and lwIP | `stubs/`. `fake_port.h` has the fake clock, WiFi status, LED and lwIP lock. `lwip/apps/mqtt.h` is the fake broker. `sample_id.h` numbers test samples (the frame has no timestamp). Used by `test_comms_cmd.c`, `test_comms_task.c`, `test_comms_threads.c`, `test_comms_frame.c` and `test_comms_fresh.c` | nothing |
| Pico W bench test (plain Pico SDK, real WiFi and real broker) | `bench/`, see `bench/README.md` | Pico W, WiFi, Mosquitto |
| Results and evidence | `TEST_LOG.md` and `logs/` | |

The PC tests with fakes prove the **logic**: retry timing, send order, acks,
commands, and link-loss time. They do not prove the real radio or a real broker.
That is what the S1-H tests on the Pico W are for.

Test IDs:
- S1-U: unit test on the PC
- S1-S: scenario
- S1-T: old text message writer (retired 2026-11-01, IDs not reused)
- S1-B: binary frame pack / unpack and command byte
- S1-L: layout table check
- S1-F: sequence, drop flag and Wi-Fi loss flag added by comms (fakes)
- S1-V: stale command and broker-down vs Wi-Fi-down cases (fakes)
- S1-D: command handling (fakes)
- S1-C: connection logic (fakes)
- S1-R: two threads
- S1-H: real Pico W bench

IDs never change and are never reused. Add new checks at the END of a test file.

Link-loss time depends on `COMMS_ACK_TIMEOUT_MS` in `comms.h` (now 3000). The tests read
that value, so they stay correct if you change it.

`test_comms_threads.c` runs two PC threads against the inbox and the command ring. It
cannot prove the memory barriers, because a PC orders memory strongly. That part is
checked by reading the code and on the board.
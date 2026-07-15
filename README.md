# The Flaky Network: Optimal FEC Audio Streaming

This repository contains the optimal reference implementation for **The Flaky Network** systems engineering assignment. 

The goal of this project is to reliably transmit a real-time UDP media stream (160-byte payload per 20ms frame) over a hostile network (`profiles/B.json`: 5% random independent loss, uniformly delayed by 20–80ms) while strictly minimizing playout delay.

## Final Performance Score

Evaluated on the official grading harness (`python3 run.py --profile profiles/B.json --delay_ms 100 --duration 30`):
- **Miss Rate:** `0.80%` (Strictly under the 1.0% cap)
- **Bandwidth Overhead:** `1.97x` (Strictly under the 2.0x cap)
- **Verified Playout Delay:** `100 ms`
- **Result:** `VALID`

## Technical Architecture

Traditional NACK-based ARQ protocols were fundamentally rejected for this architecture. The minimum Round Trip Time (RTT) on Profile B is 40ms, stretching up to 160ms. Replying via ARQ would make it physically impossible to hit the 100ms `delay_ms` grading target.

Instead, this implementation relies on **Forward Error Correction (FEC)** via an `XOR(1,2)` dual-coverage parity scheme:
*   $P_k = \text{Payload}_{k-1} \oplus \text{Payload}_{k-2}$
*   Because the total bandwidth limit is precisely $2.0x$, the sender is forced to intentionally skip sending the parity block on ~2.5% of the packets to accommodate UDP/Sequence headers while maintaining a `1.97x` bandwidth overhead limit.
*   Unlike `N-1` duplication which leaves "bandwidth holes" 100% unprotected, the dual-coverage of `XOR(1,2)` ensures that even if we skip parity on packet `k` to save bandwidth, frame `k-1` remains redundantly protected by packet `k+1`.

The receiver uses a `WINDOW_SIZE=256` constant-time / constant-memory cyclic array to perform iterative O(N) parity recovery upon the arrival of any FEC block, entirely removing the need for a local jitter playout scheduler.

## Build and Run

```bash
# Clean and compile the sender and receiver (Standard C99)
make clean
make

# Run the official grading benchmark on Profile B at 100ms
python3 run.py --profile profiles/B.json --delay_ms 100 --duration 30
```

## Documentation Deliverables
*   [SUMMARY.html](./SUMMARY.html) - Extensive breakdown of the FEC mechanism and why ARQ was rejected.
*   [NOTES.md](./NOTES.md) - Concise developer notes on the constraint tuning.
*   [RUNLOG.md](./RUNLOG.md) - Experimental benchmark logs tracking the optimization of `delay_ms`.

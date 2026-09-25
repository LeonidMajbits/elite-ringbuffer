# Live two-process throughput demonstration

Build `make CC=clang demo`, then run `build/native/live_throughput_demo`.
The same example is built and installed by CMake. Modes are `--mode spsc` and
`--mode ncq`. Both use one origin and one separately spawned executable responder,
two independent POSIX shared-memory rings, fixed 64-byte payloads, 1P/1C per
direction, and one outstanding request. NCQ here is not a contended 16P/16C test.

```sh
build/native/live_throughput_demo --mode ncq --windows 20 --messages 100000 \
  --timeout 120 --json /tmp/new-demo.jsonl
python3 tools/verify_live_demo.py /tmp/new-demo.jsonl
```

The ASCII progress bar and live numeric row redraw only when stdout is a terminal;
redirected output or `--plain` emits ordinary lines. `q` followed by Enter, SIGINT,
or SIGTERM to the origin requests stop at a completed request/reply boundary. The responder
then consumes an untimed termination control record in the request ring, ends its
aliases and detaches. The parent reconciles its sequential count, receives cleanup
receipts, detaches, waits for terminal exit, and destroys the two objects. A clean
cancel returns 130 and `CANCELLED_CLEAN`; use `--allow-cancelled` only to validate
its partial completed-record accounting, not to call it a completed plan.

## Exact metrics

A sample starts before the origin's public reserve attempt and ends after it
checks and releases the reply. It includes request construction, both handoffs,
responder read/release and reply construction, reply validation/release, and the
ordered clock boundaries. Linux uses CLOCK_MONOTONIC_RAW, Darwin uses Mach ticks
and its queried conversion, and the experimental FreeBSD path uses
CLOCK_MONOTONIC. No timer tick is relabeled as a core cycle.

The displayed p50 and p99 are exact nearest-rank order statistics of **all RTT
samples in that completed window**, converted upward to integer nanoseconds. They
are not lifetime quantiles, a pooled histogram, RTT/2, inverse throughput, or a
one-way latency certificate. A zero-tick sample is not evidence of zero physical
latency. No empirical effective-resolution or physical uncertainty certificate is
produced. Sampling and per-message time reads are part of the demo overhead.

Window throughput is `2 * completed_RTTs / (window_end - window_start)` with the
recorded timebase. The factor two is explicit: **directional payload records**
per second, not requests per second. The producer and responder write/check all
eight words. No warmup, CPU affinity, P-core assignment, or PMC collection is
claimed. Printing, sorting and JSON export occur between windows; they affect
later cache/scheduling state even though they are outside the current denominator.
Use the retained V5/V6 benchmark programs for their distinct qualification scopes.

## Replay and failure limits

The optional exclusive-new JSONL file contains header identities, per-window raw
`[start_tick, delta_ticks]` pairs, rates, percentiles and a final completed/cancelled
cleanup receipt. The verifier rejects duplicate keys, nonfinite numbers, malformed
populations, time decreases, changed ranks, inconsistent rates and missing cleanup.
Integer counts and ticks are exact. Four binary64 ULPs are permitted solely for
the displayed rate's multiply/divide/serialization rounding. This is not a
four-ULP physical clock bound. `python -O` cannot remove checks.

Storage is bounded to three arrays of at most one million 64-bit elements (24 MB)
plus the rings and OS objects. The verifier caps an individual file at 512 MiB;
large selections can exceed this replay budget and must use smaller windows/counts
or be explicitly treated as unverified demo logs. JSON writes are outside the
window and may be slow. No samples are silently dropped to meet a latency claim.

The native watchdog is cooperative (checked between windows and periodically in
empty/full loops); it is not a hard scheduler bound. CI also applies an external
owned-process timeout. Failure produces no COMPLETE record. An atexit path kills
and reaps only the demo's owned child, attempts managed object disposal, and
unlinks only known exclusively created names. A parent SIGKILL, failed partial
creation or unknown OS cleanup can still require application-owned reconciliation;
this example is not a production crash supervisor. Do not register descendants or
transfer its grants outside its private process cohort.

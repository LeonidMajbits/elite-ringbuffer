# Lab-reported Apple qualification context

These are user-supplied summaries, not native executions performed by this
packaging session, authenticated Git objects, or independently fetched raw runs.

## Latest Turn 14 receipt supplied for Turn 15

- Platform label: ZION Chassis, Apple Silicon M3 Max, bare metal.
- Parent archive: Turn14; e06484c745af29c5d74e6f0a548824dd40b015265020e80614675d39d376fec5.
- Reported make release-check: 285 unit/hardening/regression tests passed.
- Reported chaos oracle: 50 tests; formal replay oracle: 15 tests.
- Reported native smoke: 10 profiles, 156,000 measured messages.
- Reported issue fixed on the Mac: Darwin stopped/continued wait status semantics.
- Reported repository commit: a9bf0f7; not independently acquired by this release.

The archive here reconstructs the replay correction by explicitly recording and
decoding the wait-status ABI. It does not assume that a stop is terminal death.
The ten smoke profiles are not all 186 cases in the retained Linux campaign or
the larger original 100/1,000-repeat programme.

## Earlier Apple headlines retained for descriptive comparison

SPSC64-byte RTT: p50 250ns, p99 625ns, p99.99 7.21us, max1.33ms.
NCQ64-byte RTT: p50 375ns, p99 1.25us, p99.99 11.88us, max1.84ms.
SPSC native goodput: 32,066,903messages/s; NCQ1/1:26,599,872; NCQ8/8:4,105,985.
Python1-MiB helper workload: SPSC22.66GB/s, NCQ22.79GB/s (decimal payload once).
Turn11 v2 cache account: isolated2.014B RMW/s versus packed124.3M RMW/s.
Turn13 report: same seven finite-model counts repeated; 235 tests; commitcf4ec7c.

Earlier commissioning messages use both M4-family and M3 Max platform labels. No
single machine identity, raw PMC event population or controlled cross-machine
causal speedup is inferred from these summaries. Sub-microsecond RTT does not
certify the original one-way latency goals; native-helper payload work does not
measure Python-bytecode serialization.

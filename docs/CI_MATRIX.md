# CI matrix and trust boundary

`.github/workflows/ci.yml` is an executable GitHub Actions definition, not evidence
that hosted jobs have already run. Its default permissions are `contents: read`;
checkout does not persist credentials. It uses `pull_request`, not
`pull_request_target`; no secrets, write token, self-hosted runner or automatic
publication is requested. Job-wide timeouts and per-command owned process groups
bound runs. Logs and explicit failures are uploaded with `if: always()`.

| Job | Execution scope | Acceptance |
|---|---|---|
| Ubuntu / GCC | Existing Make release-check, demo, CMake, relocation | Correctness and integration, no speed threshold |
| Ubuntu / Clang | Independent compiler, same named tests | Correctness and integration |
| Ubuntu / ASan+UBSan | Instrumented core/adversarial and small demo | No findings in executed C processes |
| Ubuntu-22.04 / TSan | One-process, one-virtual-mapping thread adapter | Deliberate race detected first; clean adapter next |
| Ubuntu / Valgrind | Core and one-mapping threaded adapter | No reported errors/definite or indirect leaks |
| macOS-15 / ARM64 | Assert uname arm64, full native suite and local Homebrew tap | No Rosetta or alleged M3 Max hardware equivalence |
| FreeBSD-14.4 / AMD64 guest | Opt-in POLL_ONLY CMake, two real-process IPC tests, demo and install | Guest portability; no parkable, raw-PMC or bare-metal certification |

All action references are complete source commit IDs. The selected references are
checkout v5.0.0 (08c6903...), setup-python v6.0.0 (e797f83...), upload-artifact v6.0.0
(b7c566a...), and freebsd-vm v1.5.2 (77ed28d...). These are deliberately selected
Node24 revisions, not a claim to be each project's latest release. Dependabot
proposes reviewed changes monthly; it does not approve them or regenerate manifests.

The FreeBSD action boots QEMU on an Ubuntu host. Its OS image and pkg dependencies
are transitive download inputs, not made hermetic by pinning the JavaScript action.
The configured release is 14.4, AMD64, 2 virtual CPUs and 4 GiB. VNC/debug-on-error
is disabled, image caching disabled and source/results synchronized using rsync.
Retain actual `freebsd-version`, tool versions and the action log. Review that
third-party implementation and image custody before enabling public PR execution.

## Platform extension

The established native admission was Linux/Darwin only. FreeBSD requires an
explicit new `ELITE_EXPERIMENTAL_FREEBSD` build definition. The only native
source delta is two compile-time branches in `elite_format.c`: admit that opted-in
OS and reject PARKABLE_SPSC on FreeBSD. No queue field, memory order, payload
operation or reclamation policy changed. The Linux preprocessed source remains
byte-identical; its actual comparison is in Turn16 evidence. The formal source
binding was deliberately reviewed and updated for this conditional source delta;
that is not a newly machine-checked proof for FreeBSD. Default unopted FreeBSD
admission remains unsupported.

`ctest` on FreeBSD does not run the Linux/Darwin test_core suite that requires the
parkable API. It instead executes true POSIX polling SPSC/NCQ IPC, an explicit
unsupported-parkable contract, and both demo modes. The absence of a FreeBSD PMC
or topology adapter is not disguised as successful collection.

## Tool interpretations

TSan success concerns the documented thread adapter, not independent mmap aliases
or all interprocess executions. The activation control must emit an actual data
race report, not merely return TSan's fatal-error code. No core suppressions or
annotations invent synchronization. ASan and TSan are separate builds. Valgrind
checks selected process memory use; it cannot establish token ownership or
release/acquire correctness. A killed process does not run leak finalization.

Hosted macOS is not automatically the lab's M3 Max. Current runner labels and
hardware can change; record actual architecture and SDK. No ratio or nanosecond
threshold fails CI based on busy shared resources. Strong performance claims need
the separately retained raw-trace protocols on controlled nominated hardware.

## Local runs

```sh
python3 ci/run_lane.py --lane native --cc gcc --out build/new-gcc-ci
python3 ci/run_lane.py --lane asan --cc gcc --out build/new-asan-ci
python3 ci/run_lane.py --lane tsan --cc gcc --out build/new-tsan-ci
python3 ci/run_lane.py --lane valgrind --cc gcc --out build/new-valgrind-ci
```

Every output directory must be new. Missing Valgrind or a failed sanitizer runtime
is a failed/incomplete lane, never a fabricated pass. The macOS formula/tap and
man-page lint steps are explicit workflow steps in addition to this local runner.
Logs use a limited environment inventory; they do not dump arbitrary credentials.
The artifact stage distributes evidence to reviewers; publishing a GitHub Release,
signing a tag and approving an actual deployment remain owner-controlled actions.

The TSan job deliberately uses Ubuntu-22.04; other Ubuntu jobs use 24.04. A
sanitizer runtime startup error fails the lane rather than enabling retries,
disabling ASLR, or becoming a clean race-detector result. Exact tool versions
remain runtime evidence, not inferred from a runner label.

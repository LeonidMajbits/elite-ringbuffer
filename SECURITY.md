# Security and concurrency findings

This library connects trusted participants on one live host. A peer with a writable
mapping can corrupt it; the queue is not a sandbox or authentication boundary.
Unregistered descendants, raw-pointer use after transfer and host power-loss
persistence are outside the admitted contract.

For a memory-safety or integrity issue, use the repository's private vulnerability
reporting channel **when the owner has enabled it**. No security email or enabled
channel is asserted by this unseeded kit. Until a private owner-approved channel is
available, do not publish sensitive payloads or exploit-bearing production details
in an ordinary issue. The owner must configure contact/reporting before launch.

Provide a minimal isolated reproduction, source version/hash, compiler/runtime,
OS and architecture, expected ownership transition and observed result. Report
actual race findings separately from watchdog timeouts, sanitizer startup failures
and unqualified performance claims. No guaranteed response-time SLA, external
security audit or unsupported all-platform certification is advertised.

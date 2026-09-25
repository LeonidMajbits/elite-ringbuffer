# v1.1.0 trust capsule: integrity, not invented authorship

## What is sealed

The authoritative input is the authenticated Turn 14 archive. The delivered source
candidate is version 1.1.0, wire ABI 0x00010000. The release carries four distinct
hash domains:

1. `SOURCE_MANIFEST.sha256`: policy-defined code, tests, scripts, build input,
   license, audit and release metadata. It does not misleadingly call raw benchmark
   data source code.
2. `MANIFEST.sha256`: every regular shipped member other than itself, including
   the source manifest, all retained evidence, diagrams in Markdown and the report.
3. Report canonical hash: replace only its one declared 64-hex self-field with
   zeros; the `.md.sha256` instead hashes the literal finalized bytes.
4. Detached capsule: archive bytes/size, archive root/member count, both manifest
   digests and counts, literal/canonical report identities and release metadata.
   The capsule is outside the archive it hashes. Its own sidecar or a trusted
   externally communicated digest can be used as an integrity anchor.

No self-referential archive checksum is placed inside that archive. No detached
receipt is described as a digital signature. Hashes from the same untrusted source
can all be replaced consistently; an authentic signer requires a separately
trusted key and signing operation. This release does not generate a key under
Leon's name, attest a build host, create a remote Git object, or push `v1.1.0`.

## Verify before executing code

Run a verifier obtained through a trusted channel, not arbitrary code extracted
from an untrusted archive. The provided standalone verifier uses Python's standard
library and **does not extract or execute archive members**:

```sh
python3 tools/verify_trust_capsule.py \
  --archive /path/Elite_Systems_LockFree_RingBuffer_v1.1.0_Final_Release_Bundle.zip \
  --capsule /path/15_Turn_15_Trust_Capsule.json \
  --expected-capsule-sha256 DIGEST_FROM_THE_DETACHED_RECEIPT_OR_TRUSTED_CHANNEL
```

Without the optional externally trusted digest, the tool establishes internal
consistency, not origin. It rejects absent/duplicate/unsafe paths, symbolic links,
wrong archive root, missing inventory, changed hashes, unsupported signature
claims, nonfinite/duplicate-key JSON, wrong version and a false canonical report.
Its integer/boolean comparisons do not accept `0` in place of a false status.
`python -O` cannot remove checks. Resource caps bound member count and expanded
sizes; this is not a complete hostile-archive sandbox or protection against another
process concurrently replacing all evidence files.

After trusted extraction:

```sh
python3 tools/verify_release.py --strict
make CC=clang CXX=clang++ BUILD=build/release release-check
```

Use non-strict verification only after documented build overlays exist. Neither
verifier updates manifests or silently fixes a failed hash.

## Reproduce packaging

For release maintainers, finalize all source, docs, outcomes and report hashes;
then explicitly run `python3 tools/regenerate_manifests.py`. That command is a
write operation and is never called implicitly by verification.

```sh
python3 tools/build_release_archive.py --root . \
  --archive /outside-source/Elite_Systems_LockFree_RingBuffer_v1.1.0_Final_Release_Bundle.zip \
  --capsule /outside-source/15_Turn_15_Trust_Capsule.json
```

The packer reads only the verified inventory, sorts paths, uses one fixed archive
root and timestamp (2026-09-24 00:00:00 UTC as reproducibility metadata), and emits
regular read-only-content files with mode 0644. Invoke Python/shell scripts through
their interpreter. The fixed timestamp is not an execution timestamp. Compression
is deterministic within the same Python/zlib implementation; bit-identical compiler
builds across SDKs/absolute paths are not claimed. Existing output files are refused.
A failed packaging attempt may leave an incomplete output, never a success receipt.

The main archive contains the cumulative small/raw evidence and historical reports.
The much larger Turn 7 100M raw archive is referenced through its retained manifest
and delivery record, not silently claimed present inside this source bundle.

## Owner-controlled Git release

Merge this derivative into the verified project repository, inspect the diff and
match `release/RELEASE.json`, then run native target checks. Only the maintainer
with the actual repository and trusted signing configuration should create a signed
annotated tag. An example command after reviewing the exact commit is:

```sh
git tag -s v1.1.0 -m 'ELITEIPC v1.1.0 source release candidate'
git verify-tag v1.1.0
```

These commands were **not** executed on the user's repository by this delivery.
Do not force-move an existing tag. Publication/push and the signing identity remain
separate owner decisions. The short `a9bf0f7` supplied by the lab is recorded as
lab-reported parent context, not a Git object independently fetched by the packager.

## Primary context

NIST's definition distinguishes a digital signature's origin authentication from
bare hashing: https://csrc.nist.gov/glossary/term/digital_signature . The source
project's own mathematical and empirical claims retain the limitations of their
respective reports. A successful hash verification does not make a queue safe.

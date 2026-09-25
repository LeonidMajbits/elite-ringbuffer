#!/usr/bin/env python3
"""Verify hashes, full inventory, source coverage, and the current report.

Default mode ignores only documented generated development artifacts. --strict
is for a pristine source archive and rejects every unlisted file. Verification
never rewrites a receipt, source, manifest, or report.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import sys
# A verifier must not mutate a fresh source tree by importing local .pyc files.
sys.dont_write_bytecode = True
from release_manifest import REPORT, REQUIRED, files, source_member


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def manifest(path: Path) -> dict[str, str]:
    result = {}
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        if not match:
            raise ValueError(f'{path.name}:{number}: invalid manifest record')
        value, name = match.groups()
        rel = PurePosixPath(name)
        if (rel.is_absolute() or '..' in rel.parts or '\\' in name or
                rel.as_posix() != name or name in result or name in {'.', ''}):
            raise ValueError('unsafe/noncanonical/duplicate path: ' + name)
        result[name] = value
    if not result:
        raise ValueError('empty manifest: ' + path.name)
    return result


def verify(root: Path, strict: bool = False) -> tuple[int, int]:
    root = root.resolve(strict=True)
    full = manifest(root/'MANIFEST.sha256')
    sources = manifest(root/'SOURCE_MANIFEST.sha256')
    actual = set(files(root, strict)) - {'MANIFEST.sha256'}
    if actual != set(full):
        raise ValueError(f'inventory mismatch: unlisted={sorted(actual-set(full))}; missing={sorted(set(full)-actual)}')
    if not REQUIRED <= actual:
        raise ValueError('required release files missing: ' + repr(sorted(REQUIRED-actual)))
    for name, expected in full.items():
        path = root/name
        if not path.resolve().is_relative_to(root) or digest(path) != expected:
            raise ValueError('hash/path mismatch: ' + name)
    expected_sources = {name for name in actual if source_member(name)}
    if set(sources) != expected_sources:
        raise ValueError('source inventory does not cover the defined source policy')
    for name, expected in sources.items():
        if full[name] != expected:
            raise ValueError('source hash disagrees: ' + name)
    data = (root/REPORT).read_bytes()
    matches = list(re.finditer(rb'(?m)^\*\*Canonical-SHA256:\*\* `([0-9a-f]{64})`$', data))
    if len(matches) != 1:
        raise ValueError('canonical report field missing or duplicated')
    a, b = matches[0].span(1)
    if hashlib.sha256(data[:a]+b'0'*64+data[b:]).hexdigest().encode() != matches[0][1]:
        raise ValueError('canonical report hash mismatch')
    return len(full), len(sources)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', default='.')
    parser.add_argument('--strict', action='store_true')
    args = parser.parse_args()
    full, sources = verify(Path(args.root).resolve(strict=True), args.strict)
    print(f'PASS: {full} file hashes; {sources} source hashes; complete inventory; canonical report hash')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError) as exc:
        print('FAIL:', exc, file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Explicit release-author action, never called implicitly by verification."""
import argparse
import hashlib
from pathlib import Path
from release_manifest import files, source_member


def regenerate(root):
    inventory = files(root)
    entries = []
    for name in inventory:
        if name in {'MANIFEST.sha256', 'SOURCE_MANIFEST.sha256'}:
            continue
        with (root/name).open('rb') as stream:
            value = hashlib.file_digest(stream, 'sha256').hexdigest()
        entries.append((name, value))
    sources = [(n, h) for n, h in entries if source_member(n)]
    text = lambda records: ''.join(f'{h}  {n}\n' for n, h in records)
    source_bytes = text(sources).encode()
    (root/'SOURCE_MANIFEST.sha256').write_bytes(source_bytes)
    entries.append(('SOURCE_MANIFEST.sha256', hashlib.sha256(source_bytes).hexdigest()))
    (root/'MANIFEST.sha256').write_text(text(sorted(entries)), encoding='utf-8')
    return len(entries), len(sources)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('root', nargs='?', default='.')
    args = p.parse_args()
    print('GENERATED full/source records:', regenerate(Path(args.root).resolve(strict=True)))

#!/usr/bin/env python3
"""Package an already verified source inventory; never regenerate manifests.

Fixed sorted names, UTC ZIP timestamp, Unix modes and compression settings make
repeated packaging deterministic for the same Python/zlib implementation.
Archive and detached capsule are new files; no Git operation or signature occurs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile
sys.dont_write_bytecode = True
from verify_release import verify, manifest
from verify_trust_capsule import inspect_archive, ROOT_NAME

def build(root: Path, archive: Path, capsule: Path):
    root=root.resolve(strict=True)
    archive=archive.absolute();capsule=capsule.absolute()
    outputs=[archive,capsule,Path(str(archive)+'.sha256'),Path(str(capsule)+'.sha256')]
    if any(p.exists() for p in outputs) or len(set(outputs)) != 4:
        raise ValueError('destination exists or output aliases')
    # Do not add release output to the inventory being packaged.
    if any(p.resolve().is_relative_to(root) for p in outputs):
        raise ValueError('release outputs must be outside the source root')
    verify(root)
    paths=sorted(set(manifest(root/'MANIFEST.sha256')) | {'MANIFEST.sha256'})
    for p in outputs:p.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name in paths:
            info=zipfile.ZipInfo(ROOT_NAME+'/'+name,(2026,9,24,0,0,0))
            info.create_system=3
            info.external_attr=(0o100644<<16)
            info.compress_type=zipfile.ZIP_DEFLATED
            with (root/name).open('rb') as src,z.open(info,'w',force_zip64=True) as dst:
                while chunk:=src.read(1024*1024):dst.write(chunk)
    result=inspect_archive(archive)
    with capsule.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result,indent=2,sort_keys=True)+'\n')
    for path in (archive,capsule):
        with path.open('rb') as f:h=hashlib.file_digest(f,'sha256').hexdigest()
        with Path(str(path)+'.sha256').open('x', encoding='ascii') as stream:
            stream.write(h+'  '+path.name+'\n')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',default='.')
    p.add_argument('--archive',required=True)
    p.add_argument('--capsule',required=True)
    a=p.parse_args()
    print(json.dumps(build(Path(a.root),Path(a.archive),Path(a.capsule)),indent=2))
    return 0

if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError,zipfile.BadZipFile) as exc:
        print('FAIL:',exc,file=sys.stderr);raise SystemExit(1)

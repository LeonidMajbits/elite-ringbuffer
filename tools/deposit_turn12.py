#!/usr/bin/env python3
"""Verify and atomically copy the Turn 12 deliverables into a Drive-sync folder.
Reports local byte verification only; does not claim cloud upload/replication.
"""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import os
import shutil
import tempfile
import zipfile

ARCHIVE='Elite_Systems_LockFree_RingBuffer_Turn_12_Matrix_Bundle.zip'
REPORT='12_Turn_12_Hardware_Topology_and_Matrix_Report.md'
RECEIPT='12_Turn_12_Deposit_and_Integrity_Receipt.md'
DEFAULT_ENV=os.environ.get('GOOGLE_DRIVE_DROPZONE')
DEFAULT=Path(DEFAULT_ENV) if DEFAULT_ENV else None

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def validate(archive,expected):
    if sha(archive)!=expected:raise ValueError('archive digest mismatch')
    with zipfile.ZipFile(archive) as z:
        names=z.namelist()
        if len(names)!=len(set(names)):raise ValueError('duplicate archive entry')
        for n in names:
            p=PurePosixPath(n)
            if p.is_absolute() or '..' in p.parts or '\\' in n:raise ValueError('unsafe archive path')
        roots={PurePosixPath(n).parts[0] for n in names};
        if len(roots)!=1:raise ValueError('expected one repository root')
        root=next(iter(roots))+'/'
        raw=z.read(root+'MANIFEST.sha256').decode();entries={}
        for line in raw.splitlines():
            digest,name=line.split('  ',1)
            if name in entries:raise ValueError('duplicate manifest name')
            entries[name]=digest
        if {n[len(root):] for n in names if not n.endswith('/')}-{ 'MANIFEST.sha256' }!=set(entries):raise ValueError('manifest coverage mismatch')
        for n,h in entries.items():
            if hashlib.sha256(z.read(root+n)).hexdigest()!=h:raise ValueError('member mismatch: '+n)
        return z.read(root+REPORT)

def copy_checked(source,destination):
    source=Path(source);destination=Path(destination)
    if destination.exists():
        if destination.is_symlink() or not destination.is_file() or sha(source)!=sha(destination):raise ValueError('conflicting target: '+str(destination))
        return 'IDENTICAL'
    fd,temp=tempfile.mkstemp(prefix='.elite-turn12-',dir=destination.parent)
    try:
        with os.fdopen(fd,'wb') as out,source.open('rb') as inp:
            shutil.copyfileobj(inp,out);out.flush();os.fsync(out.fileno())
        if sha(temp)!=sha(source):raise ValueError('local copy mismatch')
        # No clobber when another publisher created the path since the check.
        os.link(temp,destination)
        return 'COPIED'
    finally:Path(temp).unlink(missing_ok=True)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path,required=True);p.add_argument('--expected-sha256',required=True);p.add_argument('--destination',type=Path,default=DEFAULT);p.add_argument('--receipt',type=Path)
    a=p.parse_args()
    if a.destination is None:raise ValueError('--destination or GOOGLE_DRIVE_DROPZONE environment variable required')
    if not a.destination.is_dir() or a.destination.is_symlink():raise ValueError('existing nonsymlink destination required')
    report=validate(a.archive,a.expected_sha256)
    with tempfile.TemporaryDirectory() as td:
        d=Path(td);rp=d/REPORT;rp.write_bytes(report)
        rs=d/(REPORT+'.sha256');rs.write_text(sha(rp)+'  '+REPORT+'\n')
        az=d/(ARCHIVE+'.sha256');az.write_text(sha(a.archive)+'  '+ARCHIVE+'\n')
        for source,name in [(a.archive,ARCHIVE),(rp,REPORT),(rs,rs.name),(az,az.name)]:print(copy_checked(source,a.destination/name),name)
        if a.receipt:print(copy_checked(a.receipt,a.destination/RECEIPT),RECEIPT)
    print('LOCAL_VERIFIED; cloud replication not observed')

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,zipfile.BadZipFile) as exc:raise SystemExit('FAIL: '+str(exc))

#!/usr/bin/env python3
"""Generate a literal Homebrew formula bound to a reviewed archive, without upload.

No repository name or published URL is invented. --local selects a file URI for
an actual local tap test; --url requires a supplied HTTPS release source URL.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path,PurePosixPath
import re
import stat
import sys
from urllib.parse import urlsplit
import zipfile
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]

def generate(repository: str, archive: Path, output: Path, url: str | None=None, local=False):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*/[A-Za-z0-9][A-Za-z0-9_.-]*',repository):
        raise ValueError('repository must be an explicit owner/name')
    archive=archive.resolve(strict=True)
    if archive.stat().st_size > 512*1024*1024:raise ValueError('source archive size limit')
    if local == bool(url):raise ValueError('choose exactly one of --local and --url')
    address=archive.as_uri() if local else url
    parsed=urlsplit(address)
    if not local and (parsed.scheme!='https' or not parsed.hostname or parsed.username or parsed.password or parsed.fragment or parsed.query):
        raise ValueError('public source needs a credential-free HTTPS URL')
    with zipfile.ZipFile(archive) as z:
        names=z.namelist()
        if len(names)>20000 or len(names)!=len(set(names)):raise ValueError('duplicate/oversize archive inventory')
        if sum(i.file_size for i in z.infolist())>2*1024**3:raise ValueError('inflated size limit')
        for info in z.infolist():
            n=info.filename;p=PurePosixPath(n)
            if p.is_absolute() or '..' in p.parts or '\\' in n or p.as_posix()!=n.rstrip('/'):
                raise ValueError('unsafe archive path')
            kind=stat.S_IFMT(info.external_attr >> 16)
            if kind not in {0,stat.S_IFREG,stat.S_IFDIR}:
                raise ValueError('nonregular source archive member')
        roots={n.split('/')[0] for n in names}
        if len(roots)!=1:raise ValueError('one archive root required')
        root=next(iter(roots))
        for member in ['CMakeLists.txt','include/elite_api.h','include/elite_version.h',
                       'examples/live_throughput_demo.c','man/eliteipc.7','LICENSE']:
            if root+'/'+member not in names:raise ValueError('missing package input '+member)
        if b'#define ELITE_VERSION_STRING "1.1.0"' not in z.read(root+'/include/elite_version.h'):
            raise ValueError('wrong native version')
    with archive.open('rb') as f:sha=hashlib.file_digest(f,'sha256').hexdigest()
    text=(ROOT/'packaging/homebrew/elite-ringbuffer.rb.in').read_text()
    # JSON string literals are also safe Ruby double-quoted strings except #{...};
    # forbid Ruby interpolation before serialization rather than trusting URL text.
    for value in [address,'https://github.com/'+repository]:
        if '#{' in value or any(ord(c)<32 for c in value):raise ValueError('unsafe formula string')
    text=text.replace('@HOMEPAGE@',json.dumps('https://github.com/'+repository)).replace('@SOURCE_URL@',json.dumps(address)).replace('@SHA256@',json.dumps(sha))
    output.mkdir(parents=True,exist_ok=False)
    (output/'elite-ringbuffer.rb').write_text(text)
    receipt={'schema':'elite-package-seed-v1','repository':repository,'source_url':address,'source_sha256':sha,
             'source_bytes':archive.stat().st_size,'version':'1.1.0','publication_status':'NOT_PUBLISHED',
             'formula_sha256':hashlib.sha256(text.encode()).hexdigest(),'local_only':local}
    (output/'PACKAGE_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repository',required=True);p.add_argument('--archive',type=Path,required=True)
    g=p.add_mutually_exclusive_group(required=True);g.add_argument('--url');g.add_argument('--local',action='store_true')
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    print(json.dumps(generate(a.repository,a.archive,a.out,a.url,a.local),indent=2));return 0
if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,OSError,zipfile.BadZipFile) as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)

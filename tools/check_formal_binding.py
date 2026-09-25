#!/usr/bin/env python3
"""Reject drift from manually reviewed native source anchors. Not an equivalence proof."""
import hashlib,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
def check(root=ROOT):
    binding=json.loads((root/'formal/SOURCE_BINDING.json').read_text())
    for name,item in binding['files'].items():
        raw=(root/name).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=item['sha256']:raise ValueError('formal/source drift: '+name)
        for anchor in item['anchors']:
            if anchor not in raw.decode():raise ValueError('missing reviewed anchor: '+name)
    return len(binding['files'])
if __name__=='__main__':
    try:print('PASS:',check(),'source files match the reviewed model binding')
    except (OSError,ValueError,KeyError) as e:print('FAIL:',e,file=sys.stderr);raise SystemExit(1)

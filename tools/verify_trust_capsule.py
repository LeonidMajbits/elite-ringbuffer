#!/usr/bin/env python3
"""Read-only, zero-dependency v1.1.0 archive/capsule verification.

This validates byte integrity, NOT an author's signature or execution attestation.
Nothing from the archive is extracted, imported, compiled, or executed. Obtain the
capsule digest from a trusted independent channel when origin matters.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import zipfile

sys.dont_write_bytecode = True
VERSION = '1.1.0'
ROOT_NAME = 'elite-ringbuffer-' + VERSION
REPORT = '15_Turn_15_Definitive_Release_Candidate_Report.md'
CORE_REQUIRED = {'LICENSE', '.gitignore', 'NOTICE.md', 'README.md', 'CHANGELOG.md',
                 'Makefile', 'release/RELEASE.json', 'include/elite_version.h',
                 'include/elite_ringbuffer.h', 'src/elite_spsc.c',
                 'src/elite_mpmc_ncq.c', 'bindings/python/elite_buffer.c',
                 'docs/ADVERSARIAL_AUDIT.md', 'SOURCE_MANIFEST.sha256', REPORT}
MAX_FILE_BYTES = 1 << 30
MAX_TOTAL_BYTES = 4 << 30
MAX_MEMBERS = 100_000

def need(condition, text):
    if not condition:
        raise ValueError(text)

def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def pairs(values):
    result = {}
    for key, value in values:
        need(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result

def json_value(data):
    return json.loads(data, object_pairs_hook=pairs,
        parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite JSON: ' + value)))

def safe_name(name):
    need(type(name) is str and 0 < len(name) < 1024, 'invalid member name')
    path = PurePosixPath(name)
    need(not path.is_absolute() and path.as_posix() == name and
         '..' not in path.parts and '\\' not in name and ':' not in name and
         all(ord(c) >= 32 and ord(c) != 127 for c in name) and name != '.',
         'unsafe/noncanonical member name: ' + name)
    return name

def parse_manifest(data):
    result = {}
    for line in data.decode('utf-8').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        need(match is not None, 'invalid manifest line')
        sha, name = match.groups()
        safe_name(name)
        need(name not in result, 'duplicate manifest member')
        result[name] = sha
    need(bool(result), 'empty manifest')
    return result

def source_member(name):
    # Kept independently explicit; a unit test compares the packaging policy.
    return PurePosixPath(name).suffix in {'.c','.h','.cpp','.hpp','.py','.sh','.pml','.rb','.cmake','.yml','.yaml','.3','.7'} or PurePosixPath(name).name in {'CMakeLists.txt','vcpkg.json'} or name.endswith(('.cmake.in','.pc.in','.rb.in')) or name in {
        'formal/SOURCE_BINDING.json', 'Makefile', 'LICENSE', '.gitignore',
        'NOTICE.md', 'docs/ADVERSARIAL_AUDIT.md', 'release/RELEASE.json'}

def inspect_archive(archive):
    archive = Path(archive)
    need(archive.is_file() and archive.stat().st_size <= MAX_TOTAL_BYTES, 'invalid archive size')
    with zipfile.ZipFile(archive) as z:
        infos = z.infolist()
        need(0 < len(infos) <= MAX_MEMBERS, 'invalid member count')
        need(sum(i.file_size for i in infos) <= MAX_TOTAL_BYTES, 'inflated-size limit')
        names = {}
        for info in infos:
            safe_name(info.filename)
            need(not info.is_dir() and not info.flag_bits & 1, 'directory/encrypted entry is not admitted')
            need(info.file_size <= MAX_FILE_BYTES, 'member size limit')
            mode = (info.external_attr >> 16) & 0o170000
            need(mode in (0, stat.S_IFREG), 'nonregular archive member')
            prefix, sep, rel = info.filename.partition('/')
            need(sep and prefix == ROOT_NAME and rel, 'wrong archive root')
            safe_name(rel)
            need(rel not in names, 'duplicate archive member')
            need(not any(part in {'build','dist','.git','__pycache__','.pytest_cache'} for part in PurePosixPath(rel).parts), 'generated member')
            names[rel] = info
        need(CORE_REQUIRED | {'MANIFEST.sha256'} <= set(names), 'required archive members missing')
        def read_small(name):
            need(names[name].file_size <= 16 * 1024 * 1024, 'metadata too large')
            return z.read(names[name])
        full_bytes = read_small('MANIFEST.sha256')
        src_bytes = read_small('SOURCE_MANIFEST.sha256')
        full, sources = parse_manifest(full_bytes), parse_manifest(src_bytes)
        need(set(full) == set(names) - {'MANIFEST.sha256'}, 'complete manifest inventory mismatch')
        need(set(sources) == {n for n in full if source_member(n)}, 'source coverage mismatch')
        measured = {}
        for name, info in names.items():
            h = hashlib.sha256()
            size = 0
            with z.open(info) as stream:
                while chunk := stream.read(1024*1024):
                    size += len(chunk)
                    need(size <= info.file_size, 'expanded member overflow')
                    h.update(chunk)
            need(size == info.file_size, 'truncated member')
            measured[name] = h.hexdigest()
            if name in full:
                need(full[name] == measured[name], 'file digest mismatch: ' + name)
            if name in sources:
                need(sources[name] == measured[name], 'source digest mismatch: ' + name)
        report = read_small(REPORT)
        matches = list(re.finditer(rb'(?m)^\*\*Canonical-SHA256:\*\* `([0-9a-f]{64})`$', report))
        need(len(matches) == 1, 'missing/duplicate report canonical field')
        a,b = matches[0].span(1)
        need(hashlib.sha256(report[:a]+b'0'*64+report[b:]).hexdigest().encode() == matches[0][1], 'canonical report mismatch')
        meta = json_value(read_small('release/RELEASE.json'))
        need(meta['version'] == VERSION and meta['tag_name'] == 'v1.1.0' and
             meta['wire_abi'] == '0x00010000' and meta['stage'] == 'release_candidate', 'release identity mismatch')
        header = read_small('include/elite_version.h').decode()
        for field,value in [('MAJOR','1'),('MINOR','1'),('PATCH','0')]:
            need(re.search(r'(?m)^#define ELITE_VERSION_'+field+r' '+value+r'$',header) is not None, 'header version mismatch')
        need('#define ELITE_VERSION_STRING "1.1.0"' in header, 'header version string mismatch')
        bound = {'MANIFEST.sha256': {'sha256':measured['MANIFEST.sha256'], 'entries':len(full)},
                 'SOURCE_MANIFEST.sha256': {'sha256':measured['SOURCE_MANIFEST.sha256'], 'entries':len(sources)},
                 REPORT: {'sha256':measured[REPORT], 'bytes':len(report), 'canonical_sha256':matches[0][1].decode()},
                 'release/RELEASE.json': {'sha256':measured['release/RELEASE.json']}}
    return {'schema':'elite-release-trust-v1', 'release_version':VERSION,
            'release_stage':'release_candidate', 'wire_abi':'0x00010000',
            'signature_status':'UNSIGNED_SHA256_RECEIPT', 'signatures':[],
            'remote_git_tag_created':False,
            'archive':{'name':archive.name, 'bytes':archive.stat().st_size,
                       'sha256':digest(archive), 'root':ROOT_NAME, 'files':len(names)},
            'bound_members':bound}

def verify(archive, capsule, expected_capsule_sha256=None):
    p = Path(capsule)
    need(p.stat().st_size <= 65536, 'capsule too large')
    if expected_capsule_sha256 is not None:
        need(re.fullmatch('[0-9a-f]{64}', expected_capsule_sha256) is not None, 'invalid expected capsule digest')
        need(digest(p) == expected_capsule_sha256, 'capsule trust-anchor mismatch')
    supplied = json_value(p.read_bytes())
    expected = inspect_archive(archive)
    need(type(supplied) is dict and json.dumps(supplied, sort_keys=True) == json.dumps(expected, sort_keys=True), 'capsule/archive mismatch or unsupported claim')
    return supplied

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive', required=True)
    p.add_argument('--capsule', required=True)
    p.add_argument('--expected-capsule-sha256')
    a = p.parse_args()
    result = verify(a.archive, a.capsule, a.expected_capsule_sha256)
    print('PASS: capsule, archive, complete/source manifests, canonical report and release identity; UNSIGNED')
    print(json.dumps(result, sort_keys=True))
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile, RuntimeError) as exc:
        print('FAIL:', exc, file=sys.stderr)
        raise SystemExit(1)

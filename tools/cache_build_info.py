#!/usr/bin/env python3
"""Emit deterministic compile provenance. Developer build tool; stdlib only."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', required=True)
    for name in ('cc', 'cppflags', 'cflags', 'ldflags', 'ldlibs'):
        p.add_argument('--' + name, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    cc = shlex.split(a.cc)
    if len(cc) != 1:
        raise ValueError('use a compiler executable, not a shell pipeline/wrapper')
    compiler = Path(shutil.which(cc[0]) or cc[0]).resolve(strict=True)
    files = sorted([x for d in ('include', 'src', 'benchmarks')
                    for x in (root/d).glob('*') if x.suffix in ('.h', '.c')] +
                   [root/'Makefile', Path(__file__).resolve()])
    hashes = {x.relative_to(root).as_posix(): hashlib.sha256(x.read_bytes()).hexdigest()
              for x in files}
    material = ''.join(f'{h}  {n}\n' for n, h in sorted(hashes.items())).encode()
    git_executable = shutil.which('git')
    git = (subprocess.run([git_executable, '-C', str(root), 'rev-parse', 'HEAD'],
                          capture_output=True, text=True) if git_executable else None)
    # A source snapshot digest is authoritative when no .git is distributed.
    data = {'compiler_path': str(compiler),
            'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
            'compiler_version': subprocess.check_output([str(compiler), '--version'], text=True).strip(),
            'cppflags': a.cppflags, 'cflags': a.cflags,
            'ldflags': a.ldflags, 'ldlibs': a.ldlibs,
            'git_commit': git.stdout.strip() if git is not None and git.returncode == 0 else None,
            'source_sha256': hashlib.sha256(material).hexdigest(), 'source_files': hashes}
    text = '#ifndef ELITE_CACHE_BUILD_INFO_H\n#define ELITE_CACHE_BUILD_INFO_H\n'
    # Adjacent literals keep lines readable, with no platform-dependent escaping.
    encoded = json.dumps(data, ensure_ascii=True, sort_keys=True, separators=(',', ':'))
    text += 'static const char *const elite_cache_build_parts[] = {\n' + ',\n'.join(json.dumps(encoded[i:i+160]) for i in range(0, len(encoded), 160)) + '\n};\n#endif\n'
    out = Path(a.out)
    if not out.exists() or out.read_text() != text:
        out.write_text(text, encoding='ascii')
    out.with_suffix('.json').write_text(json.dumps(data, indent=2, sort_keys=True)+'\n')

if __name__ == '__main__':
    main()

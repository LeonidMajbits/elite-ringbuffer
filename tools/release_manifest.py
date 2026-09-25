"""Shared release path policy; no file mutation during verification."""
from pathlib import Path
import os

REPORT = '15_Turn_15_Definitive_Release_Candidate_Report.md'
GENERATED = frozenset({'build', 'dist', '.git', '__pycache__', '.pytest_cache'})
REQUIRED = frozenset({'LICENSE', '.gitignore', 'docs/ADVERSARIAL_AUDIT.md',
                      'include/elite_version.h', 'bindings/python/elite_buffer.c',
                      'SOURCE_MANIFEST.sha256', 'src/elite_pmc.c', 'include/elite_pmc.h',
                      'tools/verify_cache_provenance.py', 'docs/CACHE_PROVENANCE.md',
                      'src/elite_topology.c', 'include/elite_topology.h',
                      'benchmarks/bench_matrix.c', 'benchmarks/matrix_bridge.c',
                      'bindings/python/matrix_worker.py', 'tools/run_matrix.py',
                      'tools/verify_matrix_results.py', 'docs/HARDWARE_BOUNDS.md',
                      'docs/MATRIX_VERIFICATION.md', 'tools/deposit_turn12.py',
                      'formal/models/spsc.py', 'formal/models/ncq.py', 'formal/handoff.py',
                      'formal/explore.py', 'formal/boundaries.py', 'formal/targeted.py',
                      'formal/spsc_ownership.pml', 'formal/ncq_sc64.pml',
                      'formal/c11/spsc_handoff.c', 'formal/c11/ncq_handoff.c',
                      'formal/SOURCE_BINDING.json', 'tools/run_formal.py',
                      'tools/verify_formal_results.py', 'tools/check_formal_binding.py',
                      'tests/test_formal.py', 'tests/test_formal_results.py',
                      'docs/FORMAL_VERIFICATION.md', 'tests/chaos_protocol.h',
                      'tests/test_chaos_multiprocess.c', 'tests/test_chaos_resources.c',
                      'tools/run_chaos.py', 'tools/verify_chaos_results.py',
                      'tools/check_chaos.py', 'tests/test_chaos_results.py',
                      'docs/CHAOS_RECOVERY.md', 'CHANGELOG.md', 'README.md',
                      'release/RELEASE.json', 'docs/TRUST_CAPSULE.md', 'docs/RELEASE_SCOPE.md',
                      'tools/verify_trust_capsule.py', 'tools/build_release_archive.py',
                      'tests/test_wait_status.py', 'tests/test_trust_capsule.py',
                      '.github/workflows/ci.yml', 'CMakeLists.txt', 'vcpkg.json', 'elite-ringbuffer.rb',
                      'man/elite_ringbuffer.3', 'man/eliteipc.7', 'examples/live_throughput_demo.c',
                      'tools/verify_live_demo.py', 'tools/seed_packages.py', 'docs/SEEDING.md', REPORT})


def files(root: Path, strict: bool = False):
    result = []
    for base, dirs, names in os.walk(root, followlinks=False):
        for name in list(dirs):
            p = Path(base, name)
            if p.is_symlink():
                raise ValueError('symlink directory: ' + str(p.relative_to(root)))
            if not strict and name in GENERATED:
                dirs.remove(name)
        for name in names:
            p = Path(base, name)
            if p.is_symlink() or not p.is_file():
                raise ValueError('nonregular release member: ' + str(p.relative_to(root)))
            rel = p.relative_to(root).as_posix()
            if not strict and (name.endswith('.pyc') or name == '.DS_Store'):
                continue
            result.append(rel)
    return sorted(result)


def source_member(name: str) -> bool:
    p = Path(name)
    return p.suffix in {'.c', '.h', '.cpp', '.hpp', '.py', '.sh', '.pml', '.rb', '.cmake', '.yml', '.yaml', '.3', '.7'} or p.name in {'CMakeLists.txt','vcpkg.json'} or name.endswith(('.cmake.in','.pc.in','.rb.in')) or name == 'formal/SOURCE_BINDING.json' or name in {
        'Makefile', 'LICENSE', '.gitignore', 'NOTICE.md', 'docs/ADVERSARIAL_AUDIT.md', 'release/RELEASE.json'}

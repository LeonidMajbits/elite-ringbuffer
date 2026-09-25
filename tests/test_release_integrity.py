#!/usr/bin/env python3
"""Verifier negative controls; use isolated fixtures, never alter the release."""
import hashlib
from pathlib import Path
import sys
import subprocess
import shutil
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from release_manifest import REPORT, REQUIRED
from regenerate_manifests import regenerate
from verify_release import verify


class ReleaseIntegrityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name).resolve()
        for name in REQUIRED - {REPORT, 'SOURCE_MANIFEST.sha256'}:
            p = self.root/name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('fixture\n')
        raw = b'# Fixture\n**Canonical-SHA256:** `' + b'0'*64 + b'`\n'
        value = hashlib.sha256(raw).hexdigest().encode()
        (self.root/REPORT).write_bytes(raw.replace(b'0'*64, value))
        regenerate(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def test_pristine_passes(self):
        verify(self.root, strict=True)

    def test_cli_is_read_only_in_pristine_tree(self):
        tools = Path(__file__).resolve().parents[1]/'tools'
        (self.root/'tools').mkdir(exist_ok=True)
        for name in ('verify_release.py', 'release_manifest.py'):
            shutil.copyfile(tools/name, self.root/'tools'/name)
        regenerate(self.root)
        before = {p.relative_to(self.root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in self.root.rglob('*') if p.is_file()}
        result = subprocess.run([sys.executable, str(self.root/'tools/verify_release.py'),
                                 str(self.root), '--strict'], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS:', result.stdout)
        after = {p.relative_to(self.root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in self.root.rglob('*') if p.is_file()}
        self.assertEqual(before, after)

    def test_required_license_edit_detected(self):
        (self.root/'LICENSE').write_text('modified\n')
        with self.assertRaises(ValueError): verify(self.root)

    def test_deleted_audit_detected(self):
        (self.root/'docs/ADVERSARIAL_AUDIT.md').unlink()
        with self.assertRaises(ValueError): verify(self.root)

    def test_unlisted_hidden_member_detected(self):
        (self.root/'.unexpected').write_text('not hashed\n')
        with self.assertRaises(ValueError): verify(self.root)

    def test_source_coverage_cannot_be_silently_dropped(self):
        p = self.root/'SOURCE_MANIFEST.sha256'
        p.write_text(''.join(x for x in p.read_text().splitlines(True) if not x.endswith('  LICENSE\n')))
        full = self.root/'MANIFEST.sha256'
        full.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  SOURCE_MANIFEST.sha256\n'
                                if x.endswith('  SOURCE_MANIFEST.sha256\n') else x
                                for x in full.read_text().splitlines(True)))
        with self.assertRaises(ValueError): verify(self.root)

    def test_symlink_rejected(self):
        (self.root/'alias').symlink_to(self.root/'LICENSE')
        with self.assertRaises(ValueError): verify(self.root)

    def test_strict_mode_rejects_build_overlay(self):
        (self.root/'build').mkdir()
        (self.root/'build/new.bin').write_bytes(b'fixture')
        verify(self.root)
        with self.assertRaises(ValueError): verify(self.root, strict=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)

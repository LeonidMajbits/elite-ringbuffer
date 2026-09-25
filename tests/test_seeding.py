"""Seeding surface checks; not a hosted Actions, FreeBSD or Homebrew execution."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import seed_packages
import release_manifest
import verify_trust_capsule

class SeedingSurface(unittest.TestCase):
    def test_workflow_pins(self):
        s=(ROOT/'.github/workflows/ci.yml').read_text()
        refs=re.findall(r'uses:\s+([^\s]+)',s)
        self.assertGreaterEqual(len(refs),8)
        for ref in refs:self.assertRegex(ref,r'^[\w.-]+/[\w.-]+@[0-9a-f]{40}$')
    def test_workflow_no_write_or_privileged_pr_trigger(self):
        s=(ROOT/'.github/workflows/ci.yml').read_text()
        self.assertNotIn('pull_request_target',s);self.assertNotIn('contents: write',s)
        self.assertNotIn('secrets.',s);self.assertNotIn('self-hosted',s)
        self.assertNotIn('continue-on-error',s)
        self.assertIn('persist-credentials: false',s);self.assertIn('if: always()',s)
    def test_matrix_scopes(self):
        s=(ROOT/'.github/workflows/ci.yml').read_text()
        for name in ['asan','tsan','valgrind','macos-15','arm64','freebsd','14.4']:
            self.assertIn(name,s)
        self.assertIn('debug-on-error: false',s)
    def test_complete_cmake_export(self):
        s=(ROOT/'CMakeLists.txt').read_text()
        for text in ['EXPORT elite_ringbufferTargets','NAMESPACE elite::','C_EXTENSIONS NO',
                     'ELITE_EXPERIMENTAL_FREEBSD','man/elite_ringbuffer.3']:
            self.assertIn(text,s)
    def test_vcpkg_metadata(self):
        for p in [ROOT/'vcpkg.json',ROOT/'packaging/vcpkg/elite-ringbuffer/vcpkg.json']:
            x=json.loads(p.read_text());self.assertEqual(x['version-semver'],'1.1.0')
            self.assertEqual(x['license'],'MIT');self.assertNotIn('windows',x['supports'])
    def test_source_manifest_policy_covers_seeding(self):
        paths=['.github/workflows/ci.yml','elite-ringbuffer.rb','cmake/elite_ringbufferConfig.cmake.in',
               'CMakeLists.txt','tests/cmake_consumer/CMakeLists.txt','vcpkg.json',
               'packaging/vcpkg/elite-ringbuffer/portfile.cmake','man/elite_ringbuffer.3','man/eliteipc.7']
        for p in paths:
            self.assertTrue(release_manifest.source_member(p),p)
            self.assertEqual(release_manifest.source_member(p),verify_trust_capsule.source_member(p))
    def test_man_pages_sections_and_contracts(self):
        for page in ['elite_ringbuffer.3','eliteipc.7']:
            s=(ROOT/'man'/page).read_text()
            for section in ['NAME','SYNOPSIS' if page.endswith('.3') else 'ARCHITECTURE','DESCRIPTION','SEE ALSO']:
                self.assertIn('.SH '+section,s)
            self.assertIn('quarantine',s.lower());self.assertIn('RTT/2',s)
            self.assertEqual(s.count('.nf'),s.count('.fi'))
    def test_manual_references_actual_api(self):
        s=(ROOT/'man/elite_ringbuffer.3').read_text();api=(ROOT/'include/elite_api.h').read_text()
        functions=set(re.findall(r'elite_[a-z_]+',s))
        for f in functions:
            if f in {'elite_ringbuffer','elite_api','eliteipc'}:continue
            self.assertIn(f,api,f)
    def test_ruby_syntax(self):
        ruby=shutil.which('ruby')
        if ruby is None:self.skipTest('Ruby parser not installed')
        r=subprocess.run([ruby,'-c',str(ROOT/'elite-ringbuffer.rb')],capture_output=True,text=True,timeout=10)
        self.assertEqual(r.returncode,0,r.stderr)

class PackageSeed(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.home=Path(self.t.name)
        self.archive=self.home/'source.zip'
        members=['CMakeLists.txt','include/elite_api.h','include/elite_version.h','examples/live_throughput_demo.c','man/eliteipc.7','LICENSE']
        with zipfile.ZipFile(self.archive,'w') as z:
            for f in members:z.writestr('elite-ringbuffer-1.1.0/'+f,(ROOT/f).read_bytes())
    def test_local_formula(self):
        out=self.home/'recipe';r=seed_packages.generate('example-owner/reviewed-repo',self.archive,out,local=True)
        self.assertEqual(r['source_sha256'],hashlib.sha256(self.archive.read_bytes()).hexdigest())
        self.assertTrue(r['local_only']);self.assertEqual(r['publication_status'],'NOT_PUBLISHED')
        s=(out/'elite-ringbuffer.rb').read_text();self.assertNotIn('@SOURCE_URL@',s);self.assertIn('file://',s)
    def test_public_formula(self):
        r=seed_packages.generate('example-owner/reviewed-repo',self.archive,self.home/'recipe',url='https://example.org/releases/source.zip')
        self.assertFalse(r['local_only'])
    def test_bad_repository(self):
        self.assertRaises(ValueError,seed_packages.generate,'x/../../evil',self.archive,self.home/'recipe',local=True)
    def test_plain_http_refused(self):
        self.assertRaises(ValueError,seed_packages.generate,'owner/repo',self.archive,self.home/'recipe',url='http://example.org/a.zip')
    def test_credentials_refused(self):
        self.assertRaises(ValueError,seed_packages.generate,'owner/repo',self.archive,self.home/'recipe',url='https://user:pass@example.org/a.zip')
    def test_no_overwrite(self):
        out=self.home/'recipe';seed_packages.generate('owner/repo',self.archive,out,local=True)
        self.assertRaises(FileExistsError,seed_packages.generate,'owner/repo',self.archive,out,local=True)
    def test_wrong_source_version(self):
        other=self.home/'bad.zip'
        with zipfile.ZipFile(self.archive) as src,zipfile.ZipFile(other,'w') as dst:
            for name in src.namelist():dst.writestr(name,src.read(name).replace(b'"1.1.0"',b'"9.9.9"'))
        self.assertRaises(ValueError,seed_packages.generate,'owner/repo',other,self.home/'recipe',local=True)

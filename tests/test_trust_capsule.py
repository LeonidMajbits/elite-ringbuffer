"""Independent release container checks, including self-consistent false claims."""
from pathlib import Path
import hashlib
import json
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from release_manifest import REQUIRED, REPORT, source_member as packaging_source
from regenerate_manifests import regenerate
import verify_trust_capsule as v
from build_release_archive import build

class CapsuleTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.home=Path(self.tmp.name)
        self.root=self.home/'source';self.root.mkdir()
        for name in (REQUIRED | v.CORE_REQUIRED) - {'SOURCE_MANIFEST.sha256',REPORT}:
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture\n')
        raw=b'# Fixture\n**Canonical-SHA256:** `'+b'0'*64+b'`\n'
        (self.root/REPORT).write_bytes(raw.replace(b'0'*64,hashlib.sha256(raw).hexdigest().encode()))
        p=self.root/'include/elite_version.h'
        p.write_text('#define ELITE_VERSION_MAJOR 1\n#define ELITE_VERSION_MINOR 1\n#define ELITE_VERSION_PATCH 0\n#define ELITE_VERSION_STRING "1.1.0"\n')
        (self.root/'release/RELEASE.json').write_text(json.dumps({'version':'1.1.0','tag_name':'v1.1.0','wire_abi':'0x00010000','stage':'release_candidate'}))
        regenerate(self.root)
        self.archive=self.home/'release.zip';self.capsule=self.home/'capsule.json'
        build(self.root,self.archive,self.capsule)
    def tearDown(self):self.tmp.cleanup()
    def entries(self):
        with zipfile.ZipFile(self.archive) as z:return [(i,z.read(i)) for i in z.infolist()]
    def rewrite(self,entries):
        out=self.home/'mutant.zip'
        with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for i,b in entries:z.writestr(i,b)
        return out
    def fails_archive(self,entries):
        with self.assertRaises((ValueError,KeyError,zipfile.BadZipFile)):v.inspect_archive(self.rewrite(entries))
    def test_valid_with_external_pin(self):v.verify(self.archive,self.capsule,v.digest(self.capsule))
    def test_wrong_pin(self):
        with self.assertRaises(ValueError):v.verify(self.archive,self.capsule,'0'*64)
    def test_capsule_modified_rate_claim(self):
        d=json.loads(self.capsule.read_text());d['performance_certified']=True;self.capsule.write_text(json.dumps(d))
        with self.assertRaises(ValueError):v.verify(self.archive,self.capsule)
    def test_signature_is_not_invented(self):
        d=json.loads(self.capsule.read_text());d['signature_status']='SIGNED';self.capsule.write_text(json.dumps(d))
        with self.assertRaises(ValueError):v.verify(self.archive,self.capsule)
    def test_bool_integer_substitution(self):
        d=json.loads(self.capsule.read_text());d['remote_git_tag_created']=0;self.capsule.write_text(json.dumps(d))
        with self.assertRaises(ValueError):v.verify(self.archive,self.capsule)
    def test_duplicate_json(self):
        self.capsule.write_text('{"release_version":"1.1.0","release_version":"1.1.0"}')
        with self.assertRaises(ValueError):v.verify(self.archive,self.capsule)
    def test_missing_member(self):self.fails_archive([(i,b) for i,b in self.entries() if not i.filename.endswith('/LICENSE')])
    def test_changed_member(self):self.fails_archive([(i,b+b'x' if i.filename.endswith('/LICENSE') else b) for i,b in self.entries()])
    def test_duplicate_archive_member(self):
        e=self.entries();self.fails_archive(e+[e[0]])
    def test_escape(self):
        i=zipfile.ZipInfo(v.ROOT_NAME+'/../escape');self.fails_archive(self.entries()+[(i,b'x')])
    def test_symlink(self):
        e=self.entries();i=zipfile.ZipInfo(v.ROOT_NAME+'/symlink');i.create_system=3;i.external_attr=(stat.S_IFLNK|0o777)<<16
        self.fails_archive(e+[(i,b'LICENSE')])
    def test_generated_file(self):
        self.fails_archive(self.entries()+[(zipfile.ZipInfo(v.ROOT_NAME+'/build/unsafe'),b'x')])
    def test_rehashed_wrong_header(self):
        p=self.root/'include/elite_version.h';p.write_text(p.read_text().replace('MINOR 1','MINOR 9'));regenerate(self.root)
        with self.assertRaises(ValueError):build(self.root,self.home/'wrong.zip',self.home/'wrong.json')
    def test_regeneration_does_not_forge_canonical(self):
        p=self.root/REPORT;p.write_bytes(p.read_bytes()+b'edited');regenerate(self.root)
        with self.assertRaises(ValueError):build(self.root,self.home/'wrong.zip',self.home/'wrong.json')
    def test_source_policy_agreement(self):
        for n in ['LICENSE','.gitignore','docs/ADVERSARIAL_AUDIT.md','README.md','a.bin','tools/a.py','release/RELEASE.json','formal/SOURCE_BINDING.json','tests/x.c']:
            self.assertEqual(v.source_member(n),packaging_source(n))
    def test_deterministic_pack(self):
        a=self.home/'second'/'release.zip';build(self.root,a,self.home/'second'/'capsule.json');self.assertEqual(v.digest(a),v.digest(self.archive))
    def test_no_overwrite(self):
        before=self.archive.read_bytes()
        with self.assertRaises(ValueError):build(self.root,self.archive,self.capsule)
        self.assertEqual(before,self.archive.read_bytes())
    def test_cli_optimized_rejects(self):
        p=subprocess.run([sys.executable,'-O',str(ROOT/'tools/verify_trust_capsule.py'),'--archive',str(self.archive),'--capsule',str(self.capsule),'--expected-capsule-sha256','0'*64],capture_output=True,text=True,timeout=20)
        self.assertEqual(p.returncode,1);self.assertIn('FAIL:',p.stderr)

if __name__=='__main__':unittest.main(verbosity=2)

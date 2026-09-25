#!/usr/bin/env python3
"""Build the optional exporter using this CPython's headers, no pip/setuptools."""
import argparse,json,pathlib,shlex,subprocess,sys,sysconfig,platform
p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--build',default='build/native');p.add_argument('--sanitize',default='',choices=['','address,undefined']);a=p.parse_args()
if platform.python_implementation()!='CPython' or sys.version_info<(3,11) or sysconfig.get_config_var('Py_GIL_DISABLED'):
    p.error('requires main-interpreter, GIL-enabled CPython >=3.11')
root=pathlib.Path(__file__).resolve().parents[1];out=(root/a.build/'python').resolve();out.mkdir(parents=True,exist_ok=True)
target=out/('_elite_buffer'+sysconfig.get_config_var('EXT_SUFFIX'))
cmd=shlex.split(a.cc)+['-std=c11','-O3','-g','-Wall','-Wextra','-Werror','-pedantic','-fPIC','-isystem',sysconfig.get_paths()['include']]
if sys.platform=='darwin':cmd+=['-bundle','-undefined','dynamic_lookup','-D_DARWIN_C_SOURCE']
else:cmd+=['-shared']
if a.sanitize:cmd+=['-O1','-fno-omit-frame-pointer','-fsanitize='+a.sanitize,'-fno-sanitize-recover=all']
cmd+=[str(root/'bindings/python/elite_buffer.c'),'-o',str(target)]
print(shlex.join(cmd),flush=True);subprocess.run(cmd,check=True)
(out/'build.json').write_text(json.dumps({'python':sys.version,'executable':sys.executable,'command':cmd,'output':str(target)},indent=2)+'\n')

#!/usr/bin/env python3
"""Install into an explicit prefix; inspect printed PYTHONPATH if not on sys.path."""
import argparse,pathlib,shutil,sys,sysconfig
p=argparse.ArgumentParser();p.add_argument('--build',required=True);p.add_argument('--prefix',required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
build=(root/a.build).resolve();prefix=pathlib.Path(a.prefix).expanduser().resolve()
site=prefix/'lib'/('python'+str(sys.version_info.major)+'.'+str(sys.version_info.minor))/'site-packages'
site.mkdir(parents=True,exist_ok=True)
package=site/'elite_ringbuffer'
shutil.copytree(root/'bindings/python/elite_ringbuffer',package,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
ext='_elite_buffer'+sysconfig.get_config_var('EXT_SUFFIX')
shutil.copy2(build/'python'/ext,site/ext)
lib='libelite_ringbuffer'+('.dylib' if sys.platform=='darwin' else '.so')
shutil.copy2(build/lib,package/lib)
print('Python package and matching native library installed to '+str(site))
print('Use this CPython version; add that directory to PYTHONPATH when needed.')

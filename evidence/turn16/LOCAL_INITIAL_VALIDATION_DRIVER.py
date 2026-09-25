from pathlib import Path
import subprocess,json,os,time,signal,hashlib
R=Path('/mnt/data/turn16_work/elite-ringbuffer-1.1.0');E=R/'evidence/turn16';records=[]
def run(name,args,env=None,timeout=600):
 print(name,flush=True)
 t=time.monotonic_ns()
 with (E/(name+'.log')).open('w') as log:
  log.write('$ '+repr(args)+'\n');log.flush()
  p=subprocess.Popen(args,cwd=R,env={**os.environ,**(env or {})},stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  timed=False
  try:rc=p.wait(timeout)
  except subprocess.TimeoutExpired:timed=True;os.killpg(p.pid,signal.SIGKILL);rc=p.wait()
 record={'name':name,'command':args,'exit':rc,'timed_out':timed,'duration_ns':time.monotonic_ns()-t,'log_sha256':hashlib.sha256((E/(name+'.log')).read_bytes()).hexdigest()}
 records.append(record);(E/'VALIDATION_RUNS.json').write_text(json.dumps(records,indent=2)+'\n')
 print(rc,flush=True)
 return rc
run('manifests-interim',['python3','tools/regenerate_manifests.py'])
run('gcc-release-check',['make','CC=gcc','CXX=g++','BUILD=build/gcc-seed','release-check','check-seeding'],timeout=1200)
run('clang-release-check',['make','CC=clang','CXX=clang++','BUILD=build/clang-seed','release-check','check-seeding'],timeout=1200)
run('cmake-gcc-ctest',['ctest','--test-dir','build/cmake-gcc-ninja','--output-on-failure'])
run('cmake-gcc-relocate',['python3','tools/check_cmake_install.py','--build','build/cmake-gcc-ninja','--work','build/gcc-relocate'])
run('cmake-clang-configure',['cmake','-S','.','-B','build/cmake-clang','-G','Ninja','-DCMAKE_C_COMPILER=clang','-DCMAKE_BUILD_TYPE=Release','-DELITE_BUILD_TESTS=ON'])
run('cmake-clang-build',['cmake','--build','build/cmake-clang','--parallel','2'])
run('cmake-clang-ctest',['ctest','--test-dir','build/cmake-clang','--output-on-failure'])
run('cmake-clang-relocate',['python3','tools/check_cmake_install.py','--build','build/cmake-clang','--work','build/clang-relocate'])
run('cmake-static-configure',['cmake','-S','.','-B','build/cmake-static','-G','Ninja','-DCMAKE_C_COMPILER=gcc','-DCMAKE_BUILD_TYPE=Release','-DELITE_BUILD_SHARED=OFF','-DELITE_BUILD_TESTS=ON'])
run('cmake-static-build',['cmake','--build','build/cmake-static','--parallel','2'])
run('cmake-static-ctest',['ctest','--test-dir','build/cmake-static','--output-on-failure'])
run('cmake-static-relocate',['python3','tools/check_cmake_install.py','--build','build/cmake-static','--work','build/static-relocate'])
(E/'VALIDATION_DONE').write_text('completed commands; inspect individual exits\n')

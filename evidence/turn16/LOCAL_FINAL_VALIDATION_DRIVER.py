from pathlib import Path
import subprocess,json,os,time,signal,hashlib
R=Path('/mnt/data/turn16_work/elite-ringbuffer-1.1.0');E=R/'evidence/turn16';records=[]
def run(name,args,env=None,timeout=1200,expected=0):
 print(name,flush=True);t=time.monotonic_ns()
 with (E/(name+'.log')).open('w') as log:
  log.write('$ '+repr(args)+'\n');log.flush()
  p=subprocess.Popen(args,cwd=R,env={**os.environ,**(env or {})},stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  timed=False
  try:rc=p.wait(timeout)
  except subprocess.TimeoutExpired:timed=True;os.killpg(p.pid,signal.SIGKILL);rc=p.wait()
 record={'name':name,'command':args,'environment_overrides':env or {},'exit':rc,'expected':expected,'timed_out':timed,'duration_ns':time.monotonic_ns()-t,'log_sha256':hashlib.sha256((E/(name+'.log')).read_bytes()).hexdigest()}
 records.append(record);(E/'FINAL_VALIDATION_RUNS.json').write_text(json.dumps(records,indent=2)+'\n');print(rc,flush=True);return rc
run('formal-current-replay',['python3','tools/verify_formal_results.py','evidence/turn16/formal_admitted'])
run('gcc-release-check-final',['make','CC=gcc','CXX=g++','BUILD=build/gcc-seed','release-check','check-seeding'])
run('clang-release-check-final',['make','CC=clang','CXX=clang++','BUILD=build/clang-seed','release-check','check-seeding'])
run('cmake-shared-configure',['cmake','-S','.','-B','build/cmake-shared','-G','Ninja','-DCMAKE_C_COMPILER=gcc','-DCMAKE_BUILD_TYPE=Release','-DELITE_BUILD_STATIC=OFF','-DELITE_BUILD_TESTS=ON'])
run('cmake-shared-build',['cmake','--build','build/cmake-shared','--parallel','2'])
run('cmake-shared-ctest',['ctest','--test-dir','build/cmake-shared','--output-on-failure'])
run('cmake-shared-relocate',['python3','tools/check_cmake_install.py','--build','build/cmake-shared','--work','build/shared-relocate'])
opts=['make','CC=gcc','BUILD=build/asan-seed','OPT=-O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-lto -fsanitize=address,undefined -fno-sanitize-recover=all','LDFLAGS=-fsanitize=address,undefined']
if run('asan-build',opts+['all','demo','build/asan-seed/test_adversarial'])==0:
 env={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
 run('asan-core',['build/asan-seed/test_core'],env)
 run('asan-adversarial',['build/asan-seed/test_adversarial'],env)
 for mode in ['spsc','ncq']:run('asan-demo-'+mode,['build/asan-seed/live_throughput_demo','--mode',mode,'--windows','2','--messages','1000','--plain'],env)
opts=['make','CC=gcc','BUILD=build/tsan-seed','OPT=-O1 -g -fno-omit-frame-pointer -fno-lto -fsanitize=thread','LDFLAGS=-fsanitize=thread']
if run('tsan-build',opts+['build/tsan-seed/test_adversarial','build/tsan-seed/tsan_detector_control'])==0:
 env={'TSAN_OPTIONS':'halt_on_error=1:exitcode=66:force_seq_cst_atomics=0:report_atomic_races=1:ignore_interceptors_accesses=0:ignore_noninstrumented_modules=0'}
 run('tsan-detector',['build/tsan-seed/tsan_detector_control'],env,expected=66)
 run('tsan-adversarial',['build/tsan-seed/test_adversarial','threads'],env)
(E/'FINAL_VALIDATION_DONE').write_text('commands completed; inspect exits and scopes\n')

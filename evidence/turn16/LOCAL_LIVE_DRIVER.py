from pathlib import Path
import json,subprocess,hashlib,sys,time,os,signal,statistics,csv
R=Path('/mnt/data/turn16_work/elite-ringbuffer-1.1.0');E=R/'evidence/turn16';out=E/'live_admitted';out.mkdir(exist_ok=False)
# This may run alongside functional checks: rates are demo observations, not a quiet-host comparison.
rows=[];records=[]
for compiler in ['gcc','clang']:
 binary=R/'build'/f'{compiler}-seed'/'live_throughput_demo'
 for mode in ['spsc','ncq']:
  stem=compiler+'-'+mode;trace=out/(stem+'.jsonl');cmd=[str(binary),'--mode',mode,'--windows','5','--messages','20000','--plain','--json',str(trace)]
  with (out/(stem+'.log')).open('w') as log:
   p=subprocess.Popen(cmd,cwd=R,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
   try:rc=p.wait(timeout=120)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);rc=p.wait()
  ver=subprocess.run([sys.executable,'tools/verify_live_demo.py',str(trace)],cwd=R,capture_output=True,text=True,timeout=30)
  (out/(stem+'-replay.log')).write_text(ver.stdout+ver.stderr)
  rec={'compiler':compiler,'mode':mode,'argv':cmd,'exit':rc,'replay_exit':ver.returncode,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest()};records.append(rec)
  if rc!=0 or ver.returncode!=0:raise SystemExit('native/replay failure: '+stem)
  data=[json.loads(s) for s in trace.read_text().splitlines()];win=data[1:-1];end=data[-1]
  rows.append({'compiler':compiler,'mode':mode,'roundtrips':end['roundtrips'],'directional_records':end['directional_records'],'windows':len(win),'median_window_mmsg_s':statistics.median(x['directional_mps']/1e6 for x in win),'median_window_p50_rtt_ns':statistics.median(x['p50_rtt_ns'] for x in win),'median_window_p99_rtt_ns':statistics.median(x['p99_rtt_ns'] for x in win)})
(out/'RUNS.json').write_text(json.dumps({'status':'PASS_WITHIN_SCOPE','conditions':records,'scope':'demonstration, no warmup, no placement, not controlled performance qualification'},indent=2)+'\n')
with (E/'LIVE_RESULTS.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
print(json.dumps(rows,indent=2))

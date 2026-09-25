#!/usr/bin/env python3
"""Differential native rational formatter check; synthetic, not timing data."""
import argparse
from fractions import Fraction
import json
import math
import random
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',required=True)
a=p.parse_args()
r=random.Random(1111)
cases=[(1,1,3,1),(0,0,1,1),((1<<64)-1,)*4,(1,1,(1<<64)-1,(1<<64)-1)]
cases += [(r.getrandbits(64),r.getrandbits(64),r.getrandbits(64) or 1,r.getrandbits(64) or 1) for _ in range(1000)]
for c in cases:
    text=subprocess.check_output([a.binary,*map(str,c)],text=True)
    exact=Fraction(c[0]*c[1],c[2]*c[3]); actual=Fraction.from_float(float(text))
    if abs(actual-exact)>Fraction.from_float(math.ulp(float(exact))):
        raise SystemExit('FAIL: native rational formatter outside one binary64 ULP')
if subprocess.run([a.binary,'1','1','0','1'],capture_output=True).returncode!=1:
    raise SystemExit('FAIL: zero denominator accepted')
print(json.dumps({'status':'PASS','synthetic_ratio_cases':len(cases),'zero_denominator_rejected':True,
                  'seed':1111,'maximum_error_bound':'one binary64 ULP'}))

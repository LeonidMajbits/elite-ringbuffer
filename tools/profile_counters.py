#!/usr/bin/env python3
"""Separate diagnostics only. Discovers tools, never guesses Apple PMU IDs.
Counter/profiler runs do not qualify unprofiled latency.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--template', help='Exact installed xctrace template name for a Darwin recording')
    p.add_argument('command', nargs=argparse.REMAINDER, help='-- EXECUTABLE ARGUMENTS...')
    a = p.parse_args()
    a.out.mkdir(mode=0o700, exist_ok=False)
    command = a.command[1:] if a.command[:1] == ['--'] else a.command
    report = {'platform': platform.platform(), 'scope': 'SEPARATE_DIAGNOSTIC_NOT_LATENCY_QUALIFICATION', 'command': command}
    if platform.system() == 'Darwin':
        tool = shutil.which('xcrun')
        if not tool:
            report['status'] = 'UNAVAILABLE_XCRUN'
        else:
            result = subprocess.run([tool, 'xctrace', 'list', 'templates'], capture_output=True, text=True, check=False)
            (a.out / 'installed-templates.txt').write_text(result.stdout + result.stderr)
            report['discovery_exit'] = result.returncode
            if a.template and command:
                if a.template not in result.stdout + result.stderr:
                    raise ValueError('template not found in installed-template discovery')
                argv = [tool, 'xctrace', 'record', '--template', a.template, '--output', str(a.out / 'counter.trace'), '--launch', '--', *command]
                with (a.out / 'record.log').open('wb') as log:
                    r = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, check=False)
                report |= {'argv': argv, 'exit_code': r.returncode, 'status': 'TRACE_REQUIRES_SCOPE_REVIEW' if r.returncode == 0 else 'PROFILE_FAILED'}
                # Spawned children may not be in a launch-scoped recording.
                # Operator must confirm recorded PIDs/CPUs/events in Instruments.
            else:
                report['status'] = 'DISCOVERY_ONLY_SELECT_INSTALLED_TEMPLATE'
    else:
        tool = shutil.which('perf')
        if not tool:
            report['status'] = 'UNAVAILABLE_PERF'
        elif not command:
            report['status'] = 'NO_WORKLOAD_SUPPLIED'
        else:
            argv = [tool, 'stat', '-x', ',', '-e', 'cycles,instructions,cache-references,cache-misses,context-switches,page-faults', '-o', str(a.out / 'perf.csv'), '--', *command]
            with (a.out / 'record.log').open('wb') as log:
                r = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, check=False)
            report |= {'argv': argv, 'exit_code': r.returncode, 'status': 'COUNTS_REQUIRE_SCOPE_AND_MULTIPLEX_REVIEW' if r.returncode == 0 else 'UNAVAILABLE_OR_PROFILE_FAILED'}
    (a.out / 'status.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'])
    return 0 if report['status'] not in ('PROFILE_FAILED',) else 1

if __name__ == '__main__':
    raise SystemExit(main())

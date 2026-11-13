#!/usr/bin/env python3
"""Capture and strictly replay native original police/crash scenarios, serially."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from agent_common import ROOT, atomic_json

# Fresh, complete-state replacements for the historical partial police probes.
CASES = [
    (case['name'], case['course'], case['seed'], case['ticks'], case['knock'], case['until_outcome'])
    for case in json.loads((ROOT / 'oracle/scenarios/police-cases.json').read_text(encoding='utf-8'))
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--case', choices=[case[0] for case in CASES])
    parser.add_argument('--recapture', action='store_true')
    args = parser.parse_args()
    controls = ROOT / 'analysis/oracle/police-zero-input.controls'
    controls.write_text('0\n' * 6001, encoding='ascii')
    for name, course, seed, frames, knock, until_outcome in CASES:
        if args.case and args.case != name:
            continue
        trace = ROOT / 'analysis/oracle' / (name + '.tsv')
        receipt = Path(str(trace) + '.receipt.json')
        environment = {key: value for key, value in os.environ.items() if not key.startswith('RR_ORACLE_')}
        expected = dict(RR_ORACLE_SIM='1', RR_ORACLE_HUMAN='1', RR_ORACLE_SOLO='0',
                        RR_ORACLE_COURSE=str(course), RR_ORACLE_LEVEL='3', RR_ORACLE_SEED=str(seed),
                        RR_ORACLE_RANDCOUNT='1', RR_ORACLE_RANDLOG='1', RR_ORACLE_RECORD='1',
                        RR_ORACLE_INPUT=str(controls), RR_ORACLE_FRAMES=str(frames))
        if knock:
            expected['RR_ORACLE_KNOCK'] = str(knock)
        if until_outcome:
            expected['RR_ORACLE_UNTIL_FINISH'] = '1'
        environment.update(expected)
        existing = json.loads(receipt.read_text()) if receipt.exists() else {}
        prior = existing.get('environment', {})
        reusable = trace.exists() and existing.get('complete') and all(
            prior.get(key) == value for key, value in expected.items()
            if key not in ('RR_ORACLE_RECORD', 'RR_ORACLE_INPUT'))
        if knock and reusable:
            # Old captures without recorded crash metadata cannot establish
            # that the native replay applied the same scenario stimulus.
            with trace.open() as source:
                reusable = any(line.startswith('input.knock_tick\t') for _, line in zip(range(100), source))
        if args.recapture or not reusable:
            print(f'Capturing {name}: intentional zero driving input for up to {frames / 60:.1f} simulation seconds'
                  + (f'; scripted crash at {knock / 60:.1f}s' if knock else '; waiting for police/RNG coverage')
                  + '. Stationary rider is expected; this is not a race-completion run.', flush=True)
            log = ROOT / 'build' / (name + '-capture.log')
            with log.open('w', encoding='utf-8') as output:
                result = subprocess.run([sys.executable, str(ROOT / 'tools/oracle_capture.py'),
                    'capture', name, '--install', 'E:/RR', '--live-hooks', str(args.helper.resolve()),
                    '--timeout', '240'], cwd=ROOT, env=environment, stdout=output, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit(f'{name}: capture failed ({result.returncode}); {log}')
        print('Strictly replaying ' + name, flush=True)
        report = ROOT / 'build' / (name + '-acceptance.json')
        result = subprocess.run([sys.executable, str(ROOT / 'tools/verify_sim.py'), '--mode', 'acceptance',
            '--driver', 'human', '--binary', str(args.binary.resolve()), '--assets', 'E:/RR',
            '--fixture', str(trace), '--timeout', '600', '--report', str(report)], cwd=ROOT)
        if result.returncode:
            raise SystemExit(f'{name}: strict replay failed ({result.returncode}); {report}')
    atomic_json(ROOT / 'build/native-police-cases-completed.json',
                {'cases': [case[0] for case in CASES if not args.case or args.case == case[0]],
                 'strict_acceptance': True})


if __name__ == '__main__':
    main()

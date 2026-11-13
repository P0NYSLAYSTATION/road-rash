#!/usr/bin/env python3
"""Regression checks for agent tooling failure contracts (synthetic traces only)."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import tracemalloc
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from agent_common import ResourceClaims, atomic_json, run_logged, source_identity
from agent_gate import ctest_counts
from agent_status import render
from build_info import identity, write_header
from oracle_capture import capture, validate_trace, validate_recorded_controls, publish_capture
from verify_sim import classify, verify

EXE = 'a' * 64


def trace(branch=None):
    result = '# rr-oracle 1\nexe\t' + EXE + '\nscenario\tsynthetic\n'
    for i in range(2):
        result += (f'frame\t{i}\ntick\t{i + 4}\nrng\t1\ncontrols\t1\n'
                   f'rider.0.position\t{i}\nrider.0.speed\t1\n')
        if branch is not None:
            result += f'player.demo_hold\t{branch}\n'
    return result + 'end\n'


class Tooling(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.env = patch.dict(os.environ, {'RR_AGENT_STATE': str(self.root / 'resources')})
        self.env.start()

    def tearDown(self):
        self.env.stop()
        self.tmp.cleanup()

    def write(self, name, text):
        p = self.root / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
        return p

    def test_capture_sequence_and_branch_evidence(self):
        p = self.write('good.tsv', trace(0))
        info = validate_trace(p, simulation=True, expected_exe=EXE, expected_frames=2)
        self.assertEqual(info['control_branch'], 'human')
        p.write_text(trace())
        self.assertEqual(validate_trace(p, True)['control_branch'], 'unverified')
        # The name cannot substitute for an instrumented branch.
        p.write_text(trace().replace('synthetic', 'sim-napa-solo-human'))
        self.assertEqual(validate_trace(p, True)['control_branch'], 'unverified')

    def test_native_companions_keep_embedded_names_and_save_bytes(self):
        from oracle_windows import collect_companions
        scratch = self.root / 'scratch'
        scratch.mkdir()
        raw = scratch / 'capture-abc-scene0.raw'
        raw.write_bytes(b'original scene')
        save = scratch / 'capture-abc.tsv-save.rrs'
        save.write_bytes(bytes(range(160)))
        controls = scratch / 'capture-abc.controls'
        controls.write_text('200\n')
        companions = collect_companions(scratch, 'capture-abc', 'current', controls)
        expected = {'capture-abc-scene0.raw', 'current-scene0.raw',
                    'current.tsv-save.rrs', 'current.controls'}
        self.assertEqual({p.name for p in companions}, expected)
        source = self.write('source.tsv', trace(0))
        output = self.root / 'accepted/current.tsv'
        receipt = publish_capture(source, output, {'complete': True}, companions)
        for name in expected:
            immutable = Path(receipt['trace']).parent / name
            self.assertEqual(immutable.read_bytes(), (output.parent / name).read_bytes())
            self.assertEqual(receipt['artifact_sha256'][name], hashlib.sha256(immutable.read_bytes()).hexdigest())
        self.assertEqual(raw.read_bytes(), b'original scene')
        self.assertEqual(save.read_bytes(), bytes(range(160)))

    def test_recorded_controls_match_hexadecimal_replay_words(self):
        p = self.write('controls.tsv', trace(0).replace('controls\t1\n', 'controls\t512\n', 1))
        recorded = self.write('record.controls', '200\n1\n')
        self.assertEqual(validate_recorded_controls(p, recorded), 2)
        for words in ['', '200\n', '512\n1\n', '200\n1\n1\n', 'bad-input\n1\n']:
            with self.subTest(words=words):
                recorded.write_text(words)
                with self.assertRaises(ValueError):
                    validate_recorded_controls(p, recorded)

    def test_incomplete_duplicate_noncontiguous_and_wrong_exe_rejected(self):
        p = self.write('bad.tsv', '')
        variants = [trace().replace('end\n', ''), trace().replace('tick\t5', 'tick\t6'),
                    trace() + 'tick\t9\n', trace().replace('rng\t1', 'rng\t1\nrng\t1', 1),
                    trace().replace('frame\t1', 'frame\t2'), trace().replace('controls\t1\n', ''),
                    trace().replace('rider.0.speed\t1', 'rider.0.speed\t18446744073709551616')]
        for text in variants:
            with self.subTest(text=text[-100:]):
                p.write_text(text)
                with self.assertRaises(ValueError):
                    validate_trace(p, True)
        p.write_text(trace())
        with self.assertRaises(ValueError):
            validate_trace(p, True, 'b' * 64)
        with self.assertRaises(ValueError):
            validate_trace(p, True, expected_frames=3)

    def test_full_race_validation_has_bounded_memory(self):
        p = self.root / 'long.tsv'
        with p.open('w') as out:
            out.write('# rr-oracle 1\nexe\t' + EXE + '\nscenario\tlong\n')
            for i in range(10000):
                out.write(f'frame\t{i}\ntick\t{i+1}\nrng\t1\ncontrols\t1\n'
                          'rider.0.position\t1\nrider.0.speed\t1\nplayer.demo_hold\t0\n')
            out.write('end\n')
        tracemalloc.start()
        try:
            info = validate_trace(p, True, EXE, 10000)
            peak = tracemalloc.get_traced_memory()[1]
        finally:
            tracemalloc.stop()
        self.assertEqual(info['tick_end'], 10000)
        self.assertEqual(info['control_branch'], 'human')
        self.assertLess(peak, 2 * 1024 * 1024)

    def test_finish_evidence_distinguishes_wreck_and_frame_limit(self):
        base = trace(0).replace('frame\t0\n', 'frame\t0\ncapture.until_finish\t1\n')
        p = self.write('finish.tsv', base)
        for outcome, finished in [(1, 0), (2, 1), (3, 1), (4, 0), (5, 0)]:
            p.write_text(base.replace('end\n', f'capture.finished\t{finished}\ncapture.outcome\t{outcome}\nend\n'))
            info = validate_trace(p, True)
            self.assertTrue(info['whole_race_requested'])
            self.assertEqual(info['race_finished'], outcome in (2, 3))
        for ending in ['', 'capture.finished\t1\ncapture.outcome\t4\n']:
            p.write_text(base.replace('end\n', ending + 'end\n'))
            with self.assertRaises(ValueError):
                validate_trace(p, True)

    def test_rng_caller_overflow_rejects_capture(self):
        p = self.write('overflow.tsv', trace(0).replace('end\n', '# RNG caller log overflow: capture rejected\nend\n'))
        with self.assertRaisesRegex(ValueError, 'instrumentation failure'):
            validate_trace(p, True)

    def test_resource_overlap_live_owner_and_explicit_release(self):
        claims = ResourceClaims()
        token = claims.acquire('first', [self.root / 'build'])
        with self.assertRaises(RuntimeError):
            claims.acquire('second', [self.root / 'build/object'])
        with self.assertRaises(RuntimeError):
            claims.acquire('second', [self.root])
        with self.assertRaises(RuntimeError):
            claims.release(token, 'second')
        with self.assertRaises(RuntimeError):
            claims.release(token, reap=True)
        other = claims.acquire('second', [self.root / 'different-build'])
        claims.release(other, 'second')
        claims.release(token, 'first')
        self.assertEqual(claims.list(), [])

    def test_claim_exception_releases_only_own_resources(self):
        claims = ResourceClaims()
        token = claims.acquire('unrelated', [self.root / 'external'])
        with self.assertRaises(ValueError):
            with claims.hold('temporary', [self.root / 'build']):
                raise ValueError('failure')
        self.assertEqual([r['token'] for r in claims.list()], [token])
        claims.release(token, 'unrelated')

    def test_dead_owner_reaping_requires_pid_check(self):
        claims = ResourceClaims()
        token = claims.acquire('owner', [self.root / 'build'])
        with patch('agent_common.pid_alive', return_value=False):
            claims.release(token, reap=True)
        self.assertEqual(claims.list(), [])

    def test_subprocess_failure_preserved_and_log_complete(self):
        log = self.root / 'command.log'
        row = run_logged([sys.executable, '-c', 'print("failure evidence"); raise SystemExit(9)'], log, self.root)
        self.assertEqual(row['exit_code'], 9)
        self.assertEqual(log.read_text(), 'failure evidence\n')
        row = run_logged([self.root / 'missing-command'], log, self.root)
        self.assertEqual(row['exit_code'], 127)

    def test_exact_diagnostic_not_any_failure(self):
        output = 'first divergence at frame 1 (tick 5): rng original 1 native 2\nrng history (last 1 ticks, golden vs native): (1,2)\n'
        rule = {'trace_sha256': EXE, 'divergence': output.splitlines()[0],
                'output_sha256': hashlib.sha256(output.encode()).hexdigest()}
        self.assertEqual(classify(1, output, rule, EXE), 'known-divergence')
        for code, text, digest in [(1, output + 'unrelated error\n', EXE),
                                   (1, output.replace('native 2', 'native 3'), EXE),
                                   (-6, output, EXE), (1, output, 'b' * 64), (0, '', EXE)]:
            self.assertEqual(classify(code, text, rule, digest), 'unexpected-failure')

    def test_acceptance_missing_demo_and_stale_receipt_fail(self):
        fixture = self.write('human.tsv', trace(1))
        with patch('verify_sim.reference_hash', return_value=EXE):
            report, code = verify(self.root / 'binary', [self.root / 'missing.tsv'], self.root)
            self.assertEqual(code, 1)
            atomic_json(str(fixture) + '.receipt.json', dict(validate_trace(fixture, True), complete=True))
            report, code = verify(self.root / 'binary', [fixture], self.root)
            self.assertEqual(code, 1)
            self.assertIn('demo_hold', report['outcomes'][0]['error'])
            fixture.write_text(trace(0))
            report, code = verify(self.root / 'binary', [fixture], self.root)
            self.assertEqual(code, 1)
            self.assertIn('stale', report['outcomes'][0]['error'])

    def test_strict_acceptance_requires_replay_capability_and_match(self):
        fixture = self.write('human.tsv', trace(0))
        atomic_json(str(fixture) + '.receipt.json', validate_trace(fixture, True))
        def fake_replay(body):
            # A shebang script is the replay binary on POSIX; Windows runs a
            # .cmd wrapper around the same script (plan 33b).
            script = self.write('replay.py', '#!' + sys.executable + '\n' + body)
            script.chmod(0o755)
            if os.name != 'nt':
                return script
            return self.write('replay.cmd', f'@"{sys.executable}" "{script}" %*\r\n')
        binary = fake_replay('import sys\n'
                             'print("human-controls-single-anchor" if "--capabilities" in sys.argv else "2 ticks identical")\n')
        with patch('verify_sim.reference_hash', return_value=EXE):
            report, code = verify(binary, [fixture], self.root)
            self.assertEqual(code, 0)
            self.assertTrue(report['acceptance_met'])
            fake_replay('print("not a real replay")\n')
            report, code = verify(binary, [fixture], self.root)
            self.assertEqual(code, 1)

    def test_replay_timeout_cannot_pass_acceptance(self):
        fixture = self.write('human.tsv', trace(0))
        atomic_json(str(fixture) + '.receipt.json', validate_trace(fixture, True))
        capability = subprocess.CompletedProcess([], 0, 'human-controls-single-anchor\n', '')
        with patch('verify_sim.reference_hash', return_value=EXE), \
             patch('verify_sim.run_binary', side_effect=[
                 capability, subprocess.TimeoutExpired('replay', 600)]) as run:
            report, code = verify(self.root / 'binary', [fixture], self.root, timeout=600)
        self.assertEqual(run.call_args.args[-1], 600)
        self.assertEqual(code, 1)
        self.assertFalse(report['acceptance_met'])
        self.assertEqual(report['counts']['unexpected-failure'], 1)
        self.assertIn('timed out', report['outcomes'][0]['error'])

    def setup_fake_capture(self, body):
        workspace = self.root / 'workspace'
        install = workspace / 'prefix/drive_c/ElectronicArts/RoadRashOracle'
        install.mkdir(parents=True)
        (workspace / 'cdrom/ROADRASH').mkdir(parents=True)
        (workspace / 'cdrom/ROADRASH/ROADRASH.EXE').write_bytes(b'reference')
        (install / 'ROADRASH.EXE').write_bytes(b'reference')
        (install / 'AWEMAN32.DLL').write_bytes(b'proxy')
        digest = hashlib.sha256(b'reference').hexdigest()
        good = trace().replace(EXE, digest)
        wine = self.write('wine', '#!' + sys.executable + '\nimport os,time\nfrom pathlib import Path\n'
                          'out=Path(os.environ["WINEPREFIX"])/"drive_c"/os.environ["RR_ORACLE_OUT"].split("\\\\")[-1]\n' +
                          body.replace('GOOD', repr(good)))
        wine.chmod(0o755)
        args = argparse.Namespace(workspace=workspace, prefix=None, install=None,
                                  output=self.root / 'accepted/scenario.tsv', wine=wine,
                                  scenario='scenario', session='test-capture', timeout=2)
        return args, digest

    @contextlib.contextmanager
    def native_capture_contract(self, outcome):
        # Synthetic files and mocked OS boundaries only. Never load the real
        # executable, registry, capture helper or DLL in these unit tests.
        install = self.root / ('native-' + outcome)
        install.mkdir()
        (install / 'ROADRASH.EXE').write_bytes(b'synthetic executable')
        (install / 'AWEMAN32.DLL').write_bytes(b'proxy')
        digest = hashlib.sha256(b'synthetic executable').hexdigest()
        args = argparse.Namespace(install=install, live_hooks=install / 'synthetic-hooks.dll',
            output=self.root / ('accepted-' + outcome) / 'scenario.tsv', scenario='scenario',
            session='test-capture', timeout=2)
        args.output.parent.mkdir()
        state = {'phase': 'prepared'}

        @contextlib.contextmanager
        def instrument(where, helper):
            self.assertEqual(where, install.resolve())
            state['phase'] = 'installed'
            try:
                yield state
            finally:
                state['phase'] = 'restored'

        def process(command, cwd, env, log, timeout):
            self.assertEqual(command, [str(install / 'ROADRASH.EXE')])
            self.assertEqual(cwd, install)
            Path(log).write_text('synthetic process log')
            Path(env['RR_ORACLE_OUT']).write_text(
                '# rr-oracle 1\n' if outcome == 'partial' else trace().replace(EXE, digest))
            return 8 if outcome == 'exit' else 0

        with patch('oracle_windows.runtime_profile', return_value={'runtime_exe_sha256': digest}), \
             patch('oracle_windows.instrument_live', instrument), \
             patch('oracle_windows.working_registry', return_value=contextlib.nullcontext('synthetic-key')), \
             patch('oracle_windows.run_owned', side_effect=process), \
             patch.dict(os.environ, {'RR_ORACLE_SIM': '1', 'RR_ORACLE_FRAMES': '2'}):
            yield args, state

    def test_failed_capture_preserves_accepted_and_releases_resources(self):
        if os.name == 'nt':
            for outcome in ['partial', 'exit']:
                with self.subTest(outcome=outcome), self.native_capture_contract(outcome) as (args, state):
                    args.output.write_text('previous accepted')
                    with self.assertRaises(ValueError):
                        capture(args)
                    self.assertEqual(args.output.read_text(), 'previous accepted')
                    self.assertEqual(state['phase'], 'restored')
                    self.assertEqual(ResourceClaims().list(), [])
                    self.assertTrue(list((args.output.parent / 'failed-captures').glob('*/failure.json')))
            return
        for body in ['out.write_text("# rr-oracle 1\\n")\n',
                     'out.write_text(GOOD)\nraise SystemExit(8)\n']:
            # Independent fake prefix per case.
            with self.subTest(body=body), tempfile.TemporaryDirectory() as tmp:
                saved = self.root
                self.root = Path(tmp)
                args, digest = self.setup_fake_capture(body)
                args.output.parent.mkdir()
                args.output.write_text('previous accepted')
                try:
                    with patch('oracle_capture.reference_hash', return_value=digest), \
                         patch.dict(os.environ, {'RR_ORACLE_SIM': '1', 'RR_ORACLE_FRAMES': '2'}):
                        with self.assertRaises(ValueError):
                            capture(args)
                    self.assertEqual(args.output.read_text(), 'previous accepted')
                    self.assertEqual(ResourceClaims().list(), [])
                    self.assertTrue(list((args.output.parent / 'failed-captures').glob('*/failure.json')))
                finally:
                    self.root = saved

    def test_capture_publish_receipt_and_immutable_run(self):
        if os.name == 'nt':
            with self.native_capture_contract('success') as (args, state), \
                 contextlib.redirect_stdout(io.StringIO()):
                capture(args)
            receipt = json.loads(Path(str(args.output) + '.receipt.json').read_text())
            self.assertTrue(receipt['complete'])
            self.assertEqual(state['phase'], 'restored')
            self.assertEqual(receipt['aweman_instrumentation']['phase'], 'restored')
            self.assertEqual(receipt['proxy_sha256'], hashlib.sha256(b'proxy').hexdigest())
            self.assertEqual(receipt['trace_sha256'], hashlib.sha256(args.output.read_bytes()).hexdigest())
            self.assertTrue(Path(receipt['trace']).exists())
            self.assertEqual(ResourceClaims().list(), [])
            return
        args, digest = self.setup_fake_capture('out.write_text(GOOD)\n')
        with patch('oracle_capture.reference_hash', return_value=digest), \
             patch.dict(os.environ, {'RR_ORACLE_SIM': '1', 'RR_ORACLE_FRAMES': '2'}), \
             contextlib.redirect_stdout(io.StringIO()):
            capture(args)
        receipt = json.loads(Path(str(args.output) + '.receipt.json').read_text())
        self.assertTrue(receipt['complete'])
        self.assertEqual(receipt['proxy_sha256'], hashlib.sha256(b'proxy').hexdigest())
        self.assertEqual(receipt['trace_sha256'], hashlib.sha256(args.output.read_bytes()).hexdigest())
        self.assertTrue(Path(receipt['trace']).exists())

    def test_source_identity_detects_untracked_changes_and_config(self):
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                        'commit', '--allow-empty', '-qm', 'initial'], check=True)
        source = self.write('src/sample.cpp', 'int x;\n')
        first = source_identity(self.root)
        self.write('docs/note.md', 'documentation only')
        self.assertEqual(first['source_sha256'], source_identity(self.root)['source_sha256'])
        source.write_text('int y;\n')
        self.assertNotEqual(first['source_sha256'], source_identity(self.root)['source_sha256'])
        header = self.root / 'generated/info.hpp'
        write_header(header, first)
        previous = header.stat().st_mtime_ns
        write_header(header, first)
        self.assertEqual(previous, header.stat().st_mtime_ns)

    def test_ctest_coverage_keeps_skips_and_failures_separate(self):
        xml = self.write('tests.xml', '<testsuite><testcase name="ok"/><testcase name="skip"><skipped/></testcase>'
                         '<testcase name="failure"><failure/></testcase></testsuite>')
        self.assertEqual(ctest_counts(xml), {'passed': 1, 'skipped': 1, 'failed': 1})

    def test_configuration_fingerprint_distinguishes_sanitizers(self):
        cache = self.write('cache.txt', 'CMAKE_CXX_FLAGS:STRING=\nRR_BUNDLE_SDL:BOOL=OFF\n')
        with patch('build_info.source_identity', return_value={'commit': 'test', 'source_sha256': EXE}):
            normal = identity('Debug', 'test-compiler', cache)
            cache.write_text('CMAKE_CXX_FLAGS:STRING=-fsanitize=address,undefined\nRR_BUNDLE_SDL:BOOL=OFF\n')
            sanitizer = identity('Debug', 'test-compiler', cache)
        self.assertNotEqual(normal['configuration_sha256'], sanitizer['configuration_sha256'])

    @staticmethod
    def unlabeled_tests(payload):
        # Plan 30 Task 1: every test needs a label from {slow, sim, disc, unit}.
        # ctest --show-only=json-v1 lists properties as [{name, value}].
        bad = []
        for t in payload.get('tests', []):
            labels = set()
            for prop in t.get('properties', []):
                if prop.get('name') == 'LABELS':
                    v = prop.get('value', [])
                    labels.update(v if isinstance(v, list) else str(v).split(';'))
            if not (labels & {'slow', 'sim', 'disc', 'unit'}):
                bad.append(t.get('name', '?'))
        return bad

    def test_ctest_labels_cover_every_test(self):
        build = os.environ.get('RR_BUILD_DIR')
        if not build:
            # Synthetic: the checker flags the unlabeled entry only.
            payload = {'tests': [{'name': 'a', 'properties': [{'name': 'LABELS', 'value': ['slow', 'sim']}]},
                                 {'name': 'b', 'properties': []}]}
            self.assertEqual(self.unlabeled_tests(payload), ['b'])
            return
        out = subprocess.run(['ctest', '--test-dir', build, '--show-only=json-v1'],
                             capture_output=True, text=True)
        self.assertEqual(out.returncode, 0, 'ctest show-only must succeed')
        bad = self.unlabeled_tests(json.loads(out.stdout))
        self.assertEqual(bad, [])

    def make_builds_repo(self):
        repo = self.root / 'repo'
        (repo / 'src').mkdir(parents=True)
        (repo / 'src' / 'x.cpp').write_text('x')
        import time
        old = time.time() - 10 * 86400
        for name in ['build-a', 'build-b', 'build-c']:
            d = repo / name
            d.mkdir()
            (d / 'f.bin').write_text('data')
        # build-a old + unclaimed; build-c fresh (now)
        os.utime(repo / 'build-a', (old, old))
        return repo

    def run_builds(self, repo, *args):
        import sys
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parent.parent / 'tools' / 'agent_workspace.py'),
                               'builds', '--root', str(repo), *args],
                              capture_output=True, text=True, env={**os.environ, 'RR_AGENT_STATE': str(self.root / 'resources')})

    def test_builds_dry_run_lists_only_prunable(self):
        from agent_common import ResourceClaims
        repo = self.make_builds_repo()
        ResourceClaims().acquire('s', [repo / 'build-b'], os.getpid())
        out = self.run_builds(repo)
        self.assertEqual(out.returncode, 0, out.stderr)
        rows = json.loads(out.stdout)
        names = [r['name'] for r in rows if r['prunable']]
        self.assertEqual(names, ['build-a'])

    def test_builds_prune_removes_only_unclaimed_old(self):
        from agent_common import ResourceClaims
        repo = self.make_builds_repo()
        ResourceClaims().acquire('s', [repo / 'build-b'], os.getpid())
        out = self.run_builds(repo, '--prune', '--yes')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertFalse((repo / 'build-a').exists())
        self.assertTrue((repo / 'build-b').exists())
        self.assertTrue((repo / 'build-c').exists())
        self.assertTrue((repo / 'src' / 'x.cpp').exists())

    def test_handoff_index_classifies_merged_and_open(self):
        from handoff_index import classify
        # merged: cited in docs text; open (resources): uncited + live PIDs.
        docs = 'see 25-1.md for the restart fix (PROGRESS.md)' 
        log = 'docs: apply 26-1.md factory order\n'
        self.assertEqual(classify('25-1.md', '# Handoff 25-1\nno pids here', docs, log), 'merged')
        self.assertEqual(classify('26-1.md', '# Handoff 26-1', docs, log), 'merged')
        self.assertEqual(classify('other.md', '# Notes\nPIDs: 1234, capture running', docs, log), 'open (resources)')
        self.assertEqual(classify('quiet.md', '# Notes\nnothing live', docs, log), 'open')

    def test_doc_checks_pass(self):
        import subprocess, sys
        out = subprocess.run([sys.executable, str(Path(__file__).resolve().parent.parent / 'tools' / 'doc_checks.py')],
                             capture_output=True, text=True)
        self.assertEqual(out.returncode, 0, out.stdout)

    def test_race_constructed_only_in_race_setup(self):
        # Plan 26 Task 4: the only production/replay Race construction lives in
        # src/race_setup.cpp; unit tests with synthetic routes are exempt.
        import re
        hits = [p for p in (Path(__file__).resolve().parent.parent / 'src').glob('*.cpp')
                if p.name != 'race_setup.cpp' and re.search(r'make_unique<\s*(rr::)?Race\s*>|\bRace\s+\w+\s*\(', p.read_text())]
        self.assertEqual(hits, [])

    def test_current_status_does_not_upgrade_fidelity_from_green_gate(self):
        output = render({'areas': [{'area': 'Physics', 'implementation': 'ported', 'verification': 'partial',
                         'remaining': 'human controls', 'evidence': []}], 'gates': {}})
        self.assertIn('partial; human controls', output)
        self.assertIn('No current receipts', output)


if __name__ == '__main__':
    unittest.main()

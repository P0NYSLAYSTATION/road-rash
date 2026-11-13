#!/usr/bin/env python3
"""Receipted original-game frontend runs; preserves shared DLL/configuration."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import tempfile
import time
import uuid
from agent_common import ROOT, ResourceClaims, atomic_json, sha256
from oracle_capture import reference_hash


def capture(args):
    if os.name == 'nt':
        return capture_native(args)
    prefix = args.prefix.resolve()
    install = prefix / 'drive_c/ElectronicArts/RoadRashOracle'
    dll = install / 'AWEMAN32.DLL'
    ini = install / 'ddraw.ini'
    exe = install / 'ROADRASH.EXE'
    if sha256(exe) != reference_hash():
        raise ValueError('reference EXE hash mismatch')
    script = args.script.read_bytes()
    labels = re.findall(rb'^capture ([A-Za-z0-9_-]+)\s*$', script, re.M)
    if not labels or b'exit' not in script:
        raise ValueError('script requires captures and explicit exit')
    run = args.output.resolve() / uuid.uuid4().hex
    run.mkdir(parents=True)
    tag = 'fe-' + run.name
    croot = prefix / 'drive_c'
    script_path = croot / (tag + '.script')
    trace_path = croot / (tag + '.log')
    with ResourceClaims().hold('frontend-acceptance', [prefix, args.output]):
        saved_dll, saved_ini = dll.read_bytes(), ini.read_bytes()
        proc = None
        try:
            script_path.write_bytes(script)
            shutil.copy2(ROOT / 'oracle/build/AWEMAN32.DLL', dll)
            text = saved_ini.decode()
            for key, value in [('width','640'),('height','480'),('renderer','gdi'),('devmode','true')]:
                text = re.sub(r'(?m)^'+key+r'=[^\r\n]*',key+'='+value,text)
            ini.write_bytes(text.encode())
            env = {k:v for k,v in os.environ.items() if not k.startswith('RR_ORACLE_')}
            env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all', WINEDLLOVERRIDES='ddraw=n,b;aweman32=n',
                       RR_ORACLE_FRONTEND='1', RR_ORACLE_DRIVE=str(args.drive), RR_ORACLE_FRONTEND_SCRIPT='C:\\'+script_path.name,
                       RR_ORACLE_OUT='C:\\'+trace_path.name, RR_ORACLE_SCENARIO=tag)
            started=time.time()
            with (run/'wine.log').open('w') as out:
                proc=subprocess.Popen([str(args.wine),str(exe)],cwd=install,env=env,stdout=out,
                                      stderr=subprocess.STDOUT,start_new_session=True)
                try:
                    code=proc.wait(timeout=args.timeout)
                except (subprocess.TimeoutExpired,KeyboardInterrupt):
                    os.killpg(proc.pid,signal.SIGTERM)
                    try: proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid,signal.SIGKILL);proc.wait()
                    raise RuntimeError('frontend capture interrupted/timed out')
            trace=trace_path.read_text(errors='replace')
            (run/'trace.log').write_text(trace)
            if code or '# fe complete' not in trace or '# fe FAIL' in trace:
                raise RuntimeError('original frontend did not complete successfully')
            artifacts=[]
            for path in croot.glob('fe-*.RRS'):
                if path.stat().st_mtime >= started:
                    dest=run/path.name;shutil.copy2(path,dest);artifacts.append(dest)
            for rawlabel in labels:
                label=rawlabel.decode()
                if '# fe captured '+label+' 1 screen ' not in trace:
                    raise RuntimeError('capture missing/invalid: '+label)
                for path in croot.glob('fe-'+label+'.*'):
                    if path.stat().st_mtime < started: continue
                    dest=run/path.name;shutil.copy2(path,dest);artifacts.append(dest)
                for path in croot.glob('fe-'+label+'-cache*.raw'):
                    if path.stat().st_mtime < started: continue
                    dest=run/path.name;shutil.copy2(path,dest);artifacts.append(dest)
            atomic_json(run/'receipt.json',dict(exe_sha256=sha256(exe),proxy_sha256=sha256(dll),
                script_sha256=hashlib.sha256(script).hexdigest(),script=script.decode(),
                exit_code=code,duration_seconds=time.time()-started,
                artifacts={p.name:sha256(p) for p in artifacts},complete=True))
            print(run)
        finally:
            dll.write_bytes(saved_dll);ini.write_bytes(saved_ini)
            if dll.read_bytes()!=saved_dll or ini.read_bytes()!=saved_ini:
                raise RuntimeError('original configuration restoration failed')


@contextlib.contextmanager
def native_menu_renderer(install):
    """Expose the original menu surface to BitBlt; restore every INI byte."""
    ini = install / 'ddraw.ini'
    saved = ini.read_bytes()
    original_hash = hashlib.sha256(saved).hexdigest()
    text = saved.decode('utf-8')
    settings = {'width':'640', 'height':'480', 'fullscreen':'false', 'windowed':'true',
                'renderer':'gdi', 'devmode':'true', 'savesettings':'0'}
    # Apply to existing per-game sections too: they override [ddraw].
    for key, value in settings.items():
        text, count = re.subn(r'(?m)^' + re.escape(key) + r'=[^\r\n]*', key + '=' + value, text)
        if not count:
            text = text.replace('[ddraw]', '[ddraw]\n' + key + '=' + value, 1)
    state = {'original_sha256':original_hash, 'phase':'prepared'}
    try:
        ini.write_bytes(text.encode('utf-8'))
        state['capture_sha256'] = sha256(ini)
        state['phase'] = 'installed'
        yield state
    finally:
        ini.write_bytes(saved)
        if sha256(ini) != original_hash:
            raise RuntimeError('Working renderer configuration restoration failed')
        state['phase'] = 'restored'


def capture_native(args):
    """Run the existing frontend script in the owner's exact working install."""
    from oracle_windows import WORKING_INSTALL, instrument_live, runtime_profile, run_owned, working_registry
    install = args.install.resolve()
    profile = runtime_profile(install / 'ROADRASH.EXE')
    script = args.script.read_bytes()
    labels = re.findall(rb'^(capture|state) ([A-Za-z0-9_-]+)\s*$', script, re.M)
    if not labels or not re.search(rb'^exit\s*$', script, re.M):
        raise ValueError('script requires captures and explicit exit')
    if args.timeout <= 0 or not args.live_hooks:
        raise ValueError('positive timeout and --live-hooks are required')
    run = args.output.resolve() / uuid.uuid4().hex
    run.mkdir(parents=True)
    with ResourceClaims().hold('frontend-acceptance', [WORKING_INSTALL, args.output]):
        with tempfile.TemporaryDirectory(prefix='rr-frontend-') as temporary:
            scratch = Path(temporary)
            script_path = scratch / 'frontend.script'
            # Save-dialog destinations are confined to capture-owned files.
            # Script arguments are logical filenames, not user-save paths.
            text = script.decode('utf-8')
            for name in re.findall(r'^saveas\s+(\S+)\s*$', text, re.M):
                if not re.fullmatch(r'fe-[A-Za-z0-9_-]+\.[Rr][Rr][Ss]', name):
                    raise ValueError('saveas requires a capture-owned fe-*.RRS filename')
                text = re.sub(r'^saveas\s+' + re.escape(name) + r'\s*$',
                              lambda _: 'saveas ' + str(scratch / name), text, flags=re.M)
            script_path.write_text(text, encoding='utf-8')
            trace_path = scratch / 'frontend.log'
            started = time.time()
            instrumentation = {}
            code = None
            try:
                with (native_menu_renderer(install) if any(kind == b'capture' for kind, _ in labels)
                      else contextlib.nullcontext({'mode':'unchanged-state-only'})) as renderer, \
                     instrument_live(install, args.live_hooks.resolve()) as instrumentation:
                    with working_registry(install) as registry_key:
                        env = {k:v for k,v in os.environ.items() if not k.startswith('RR_ORACLE_')}
                        env.update(RR_ORACLE_FRONTEND='1', RR_ORACLE_DRIVE=str(args.drive),
                                   RR_ORACLE_FRONTEND_SCRIPT=str(script_path),
                                   RR_ORACLE_FRONTEND_DIRECTORY=str(scratch),
                                   RR_ORACLE_OUT=str(trace_path), RR_ORACLE_SCENARIO='frontend-' + run.name,
                                   RR_ORACLE_EXE_SHA256=profile['runtime_exe_sha256'],
                                   RR_ORACLE_REGISTRY_KEY=registry_key, RR_ORACLE_NONINTERACTIVE='1')
                        code = run_owned([str(install / 'ROADRASH.EXE')], install, env,
                                         run / 'process.log', args.timeout)
                        trace = trace_path.read_text(errors='replace') if trace_path.exists() else ''
                        (run / 'trace.log').write_text(trace, encoding='utf-8')
                        if code or '# fe complete' not in trace or '# fe FAIL' in trace:
                            raise RuntimeError('original frontend did not complete successfully')
                        for kind_bytes, label_bytes in labels:
                            kind, label = kind_bytes.decode(), label_bytes.decode()
                            if kind == 'state':
                                if '# fe state ' + label + ' screen ' not in trace or not (scratch / ('fe-' + label + '.state')).is_file():
                                    raise RuntimeError('state capture missing: ' + label)
                                continue
                            if '# fe captured ' + label + ' 1 screen ' not in trace:
                                raise RuntimeError('capture missing/invalid: ' + label)
                            for suffix in ('.bmp', '.state'):
                                if not (scratch / ('fe-' + label + suffix)).is_file():
                                    raise RuntimeError('capture companion missing: ' + label + suffix)
                            bitmap = (scratch / ('fe-' + label + '.bmp')).read_bytes()
                            if len(bitmap) <= 54 or bitmap[:2] != b'BM' or not any(bitmap[54:]):
                                raise RuntimeError('capture has no visible menu pixels: ' + label)
                # Only publish accepted artifacts after working-DLL restoration.
                artifacts = {}
                for path in scratch.glob('fe-*'):
                    if path.is_file():
                        shutil.copy2(path, run / path.name)
                        artifacts[path.name] = sha256(run / path.name)
                atomic_json(run / 'receipt.json', dict(profile, platform='native-windows',
                    exe_sha256=profile['runtime_exe_sha256'], aweman_instrumentation=instrumentation,
                    renderer_configuration=renderer,
                    helper_sha256=sha256(args.live_hooks), script_sha256=hashlib.sha256(script).hexdigest(),
                    script=script.decode('utf-8'), exit_code=code, duration_seconds=time.time()-started,
                    artifacts=artifacts, complete=True))
            except BaseException as error:
                if trace_path.exists():
                    shutil.copy2(trace_path, run / 'trace.log')
                for path in scratch.glob('fe-*'):
                    if path.is_file():
                        shutil.copy2(path, run / path.name)
                atomic_json(run / 'failure.json', dict(error=str(error),
                    exit_code=code, script=script.decode('utf-8'),
                    aweman_instrumentation=instrumentation, complete=False))
                raise
    print(run)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('script',type=Path)
    p.add_argument('--prefix',type=Path,default=ROOT.parent/'prefix')
    p.add_argument('--wine',type=Path,default=Path.home()/'Applications/Wine Staging.app/Contents/Resources/wine/bin/wine')
    p.add_argument('--output',type=Path,default=ROOT/('analysis/frontend/acceptance-native' if os.name == 'nt'
                                                  else 'analysis/frontend/acceptance-20261026'))
    p.add_argument('--install',type=Path,default=Path('E:/RR'))
    p.add_argument('--live-hooks',type=Path)
    p.add_argument('--timeout',type=float,default=600)
    p.add_argument('--drive',type=int,choices=[0,1,2,3,4,5],default=0)
    capture(p.parse_args())

if __name__=='__main__': main()

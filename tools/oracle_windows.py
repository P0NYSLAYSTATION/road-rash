"""Native capture in the exact working E:/RR installation, with reversible DLL hooks."""
import contextlib
import ctypes
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import uuid
from agent_common import ROOT, ResourceClaims, atomic_json, sha256

REFERENCE_SHA='73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2'
COMPATIBLE_SHA='b845fc4053f2387c863d3fd71d4b1457cefc2eab900d9562511d8d403b6f7563'

WORKING_INSTALL = Path('E:/RR')
WORKING_DLL_SHA = 'ca7f2f65bf6d1550857556a27d65d4f44d7a6ca14c2be89b7c0f9affb6a47022'

def runtime_profile(exe):
    exe=Path(exe).resolve()
    if str(exe).casefold()!=str((WORKING_INSTALL/'ROADRASH.EXE').resolve()).casefold():
        raise ValueError('Native capture must use E:/RR/ROADRASH.EXE in that exact installation')
    digest=sha256(exe)
    if digest!=COMPATIBLE_SHA:raise ValueError('The working executable changed; capture profile needs review')
    return {'runtime_exe_sha256':digest,'compatibility_patch':{'file_offset':0x48bc0,'reference_bytes':'81ec04010000','runtime_bytes':'9090909090c3','scope':'startup compatibility check; simulation instructions unchanged'}}


def require_x86_dll(path):
    data=Path(path).read_bytes()
    if len(data)<64 or data[:2]!=b'MZ':raise ValueError('Invalid capture DLL')
    offset=struct.unpack_from('<I',data,0x3c)[0]
    if offset+24>len(data) or data[offset:offset+4]!=b'PE\0\0' or struct.unpack_from('<H',data,offset+4)[0]!=0x14c:
        raise ValueError('The original game needs a 32-bit x86 capture DLL')

def copy_owned(source,destination):
    destination=Path(destination)
    if destination.exists():destination.chmod(0o666)
    result=shutil.copy2(source,destination)
    destination.chmod(0o666)
    return result

def run_owned(command,cwd,env,log,timeout):
    with Path(log).open('w',encoding='utf-8') as output:
        process=subprocess.Popen(command,cwd=cwd,env=env,stdout=output,stderr=subprocess.STDOUT)
        try:return process.wait(timeout=timeout)
        except (subprocess.TimeoutExpired,KeyboardInterrupt):
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
            return 124

@contextlib.contextmanager
def instrument_live(install,helper):
    from instrument_aweman import instrument
    require_x86_dll(helper)
    target=install/'AWEMAN32.DLL';helper_target=install/'RRCAPTURE.DLL'
    backup=ROOT/'artifacts/oracle-live-backups'/uuid.uuid4().hex;backup.mkdir(parents=True)
    original_hash=sha256(target);original_stat=target.stat()
    if original_hash!=WORKING_DLL_SHA:
        raise ValueError('E:/RR/AWEMAN32.DLL is not the verified working DLL; refusing to instrument it')
    shutil.copy2(target,backup/'AWEMAN32.DLL')
    existing_helper=helper_target.exists()
    if existing_helper:shutil.copy2(helper_target,backup/'RRCAPTURE.DLL')
    info=instrument(target,backup/'AWEMAN32-instrumented.DLL')
    info.update(backup=str(backup),helper_sha256=sha256(helper),phase='prepared')
    atomic_json(backup/'receipt.json',info)
    installed=False
    try:
        copy_owned(helper,helper_target)
        copy_owned(backup/'AWEMAN32-instrumented.DLL',target);installed=True
        info['phase']='installed';atomic_json(backup/'receipt.json',info)
        yield info
    finally:
        if installed or sha256(target)!=original_hash:
            current=sha256(target)
            if current not in {original_hash,info['instrumented_dll_sha256']}:
                raise RuntimeError('Working AWEMAN changed externally; original backup is '+str(backup))
            copy_owned(backup/'AWEMAN32.DLL',target)
            os.utime(target,ns=(original_stat.st_atime_ns,original_stat.st_mtime_ns))
            kernel=ctypes.WinDLL('kernel32',use_last_error=True)
            kernel.SetFileAttributesW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32]
            if not kernel.SetFileAttributesW(str(target),original_stat.st_file_attributes):raise ctypes.WinError(ctypes.get_last_error())
        if existing_helper:copy_owned(backup/'RRCAPTURE.DLL',helper_target)
        elif helper_target.exists() and sha256(helper_target)==info['helper_sha256']:helper_target.unlink()
        if sha256(target)!=original_hash:raise RuntimeError('Working AWEMAN restoration checksum failed')
        info['phase']='restored';atomic_json(backup/'receipt.json',info)


@contextlib.contextmanager
def working_registry(install):
    import winreg
    path=r'Software\RoadRashOracle\Runs'+chr(92)+uuid.uuid4().hex
    settings={}
    sources=[(winreg.HKEY_LOCAL_MACHINE,r'Software\Electronic Arts\RoadRash 95'),
             (winreg.HKEY_CURRENT_USER,r'Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Electronic Arts\RoadRash 95'),
             (winreg.HKEY_CURRENT_USER,r'Software\Electronic Arts\RoadRash 95')]
    for hive,source in sources:
        try:
            with winreg.OpenKey(hive,source,0,winreg.KEY_READ|winreg.KEY_WOW64_32KEY) as key:
                for index in range(winreg.QueryInfoKey(key)[1]):
                    name,value,kind=winreg.EnumValue(key,index);settings[name]=(value,kind)
        except OSError:continue
    # Keep captures independent of machine-key write permissions. The original
    # requests KEY_ALL_ACCESS and otherwise exits with registry error 5.
    settings['Path']=(str(install),winreg.REG_SZ)
    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER,path,0,winreg.KEY_ALL_ACCESS|winreg.KEY_WOW64_32KEY) as key:
        for name,(value,kind) in settings.items():winreg.SetValueEx(key,name,0,kind,value)
    try:yield path
    finally:winreg.DeleteKeyEx(winreg.HKEY_CURRENT_USER,path,winreg.KEY_WOW64_32KEY)

def capture(args):
    from oracle_capture import validate_trace,publish_capture
    import re
    if os.name!='nt':raise ValueError('Native capture requires Windows')
    if not re.fullmatch(r'[A-Za-z0-9_-]+',args.scenario):raise ValueError('Invalid scenario name')
    if args.timeout<=0:raise ValueError('Timeout must be positive')
    install=(args.install or WORKING_INSTALL).resolve()
    if not args.live_hooks:raise ValueError('Supply --live-hooks to instrument E:/RR/AWEMAN32.DLL')
    profile=runtime_profile(install/'ROADRASH.EXE')
    output=(args.output or ROOT/'analysis/oracle'/(args.scenario+'.tsv')).resolve()
    input_path=Path(os.environ['RR_ORACLE_INPUT']).resolve() if os.environ.get('RR_ORACLE_INPUT') else None
    input_hash=sha256(input_path) if input_path else None
    with ResourceClaims().hold(args.session,[install,output.parent]):
        with tempfile.TemporaryDirectory(prefix='rr-oracle-native-') as temporary:
            scratch=Path(temporary);tag='capture-'+uuid.uuid4().hex;trace=scratch/(tag+'.tsv')
            log=scratch/'process.log';controls=scratch/(tag+'.controls')
            instrumentation={};code=None;started=time.time()
            try:
                with instrument_live(install,args.live_hooks.resolve()) as instrumentation:
                    with working_registry(install) as registry_key:
                        dll_hash=sha256(install/'AWEMAN32.DLL')
                        env=dict(os.environ,RR_ORACLE_OUT=str(trace),RR_ORACLE_SCENARIO=tag,
                            RR_ORACLE_EXE_SHA256=profile['runtime_exe_sha256'],
                            RR_ORACLE_REGISTRY_KEY=registry_key,RR_ORACLE_NONINTERACTIVE='1')
                        if input_path:env['RR_ORACLE_INPUT']=str(input_path)
                        record=bool(env.get('RR_ORACLE_RECORD'))
                        if record:env['RR_ORACLE_RECORD']=str(controls)
                        code=run_owned([str(install/'ROADRASH.EXE')],install,env,log,args.timeout)
                        if code:raise ValueError(f'Original Windows game exited {code}')
                        if sha256(install/'AWEMAN32.DLL')!=dll_hash:raise ValueError('Capture DLL changed during execution')
                        if input_path and sha256(input_path)!=input_hash:raise ValueError('Controls changed during execution')
                        info=validate_trace(trace,env.get('RR_ORACLE_SIM','0')!='0',
                            profile['runtime_exe_sha256'],int(env['RR_ORACLE_FRAMES'])
                            if env.get('RR_ORACLE_FRAMES') and env.get('RR_ORACLE_UNTIL_FINISH','0')=='0' else None)
                        if env.get('RR_ORACLE_FRAMES') and info['frames'] > int(env['RR_ORACLE_FRAMES']):
                            raise ValueError('Capture exceeded requested tick limit')
                        if record and not controls.is_file():raise ValueError('Recorded controls missing')
                        if env.get('RR_ORACLE_HUMAN','0')!='0' and info['control_branch']!='human':
                            raise ValueError('Capture does not prove execution of the original human-control branch')
                # Do not accept or replace any output until restoration succeeded.
                companions=[]
                for file in list(scratch.glob(tag+'-*.raw'))+([controls] if controls.is_file() else []):
                    target=scratch/file.name.replace(tag,output.stem,1)
                    shutil.copy2(file,target);companions.append(target)
                info.update(reference_exe_sha256=REFERENCE_SHA,**profile,
                    aweman_instrumentation=dict(instrumentation),requested_scenario=args.scenario,
                    proxy_sha256=dll_hash,process_exit_code=code,duration_seconds=time.time()-started,
                    controls_sha256=sha256(controls) if controls.is_file() else input_hash,
                    platform='native-windows',environment={k:v for k,v in env.items() if k.startswith('RR_ORACLE_')})
                receipt=publish_capture(trace,output,info,companions)
                shutil.copy2(log,Path(receipt['trace']).parent/'process.log')
            except Exception as error:
                failed=output.parent/'failed-captures'/tag;failed.mkdir(parents=True)
                if log.exists():shutil.copy2(log,failed/'process.log')
                if trace.exists():shutil.copy2(trace,failed/'partial.tsv')
                atomic_json(failed/'failure.json',{'error':str(error),'exit_code':code,
                    'aweman_instrumentation':dict(instrumentation)})
                raise
    print(json.dumps(receipt,indent=2))

"""Package only executables, redistributable libraries, notices and tools."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ['rr-game.exe', 'libfluidsynth-3.dll', 'sndfile.dll', 'SDL3.dll']
NOTICES = ['SDL-LICENSE.txt', 'LICENSE', 'libdatachannel-LICENSE.txt',
           'libjuice-LICENSE.txt', 'usrsctp-LICENSE.md', 'plog-LICENSE.txt',
           'OpenSSL-LICENSE.txt', 'README.md', 'fluidsynth-COPYING.txt',
           'libsndfile-COPYING.txt']

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', default='build/release')
    parser.add_argument('--version', default='1.0.0')
    parser.add_argument('--iscc', default='deps/inno/ISCC.exe')
    args = parser.parse_args()
    build = (ROOT / args.build_dir).resolve()
    stage = ROOT / 'artifacts/installer-stage'
    stage.mkdir(parents=True, exist_ok=True)
    for name in RUNTIME:
        source = build / name
        if not source.is_file():
            raise SystemExit('Missing runtime: ' + str(source))
        shutil.copy2(source, stage / name)
    shutil.copy2(ROOT / 'tools/extract_3do_audio.py', stage / 'extract_3do_audio.py')
    licenses = stage / 'licenses'
    licenses.mkdir(exist_ok=True)
    for name in NOTICES:
        shutil.copy2(ROOT / 'third_party' / name, licenses / name)
    info = subprocess.check_output([str(build / 'rr-game.exe'), '--build-info'], text=True)
    json.loads(info)
    (stage / 'BUILD_INFO.json').write_text(info)
    (stage / 'README.txt').write_text('''Road Rash 3DO Audio Port

Requires assets from your own PC installation. Optional 3DO WAVs are imported
locally. No game assets or soundfonts are distributed.

Put an SF2, SF3 or DLS bank beside rr-game.exe or in soundfonts/ to override
Windows gm.dls. Saves are in saves/ and are loaded from the in-game menus.
rr-port.ini selects audio=3do or audio=pc and renderer=dx9 or renderer=sdl.
A missing or invalid 3DO bank automatically retains PC audio.

Source and build instructions: https://github.com/P0NYSLAYSTATION/road-rash
Fidelity details: docs/FORK_AUDIO.md in that repository.
BUILD_INFO.json identifies the source revision used for this executable.

FluidSynth 2.6.2 and libsndfile 1.2.2 use replaceable dynamic libraries (LGPL).
Licenses and source notices are in licenses/. Compatible modified DLLs may
replace the bundled libraries.
https://github.com/FluidSynth/fluidsynth/tree/v2.6.2
https://github.com/libsndfile/libsndfile/tree/1.2.2
''')
    # An allowlist also rejects stale commercial files from an earlier stage.
    allowed = set(RUNTIME + ['README.txt', 'BUILD_INFO.json', 'extract_3do_audio.py'])
    allowed.update('licenses/' + name for name in NOTICES)
    for file in stage.rglob('*'):
        if file.is_file() and file.relative_to(stage).as_posix() not in allowed:
            raise SystemExit('Unexpected file in asset-free installer stage: ' + str(file))
    output = ROOT / 'artifacts/release'
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str((ROOT / args.iscc).resolve()), '/DSourceDir=' + str(stage),
                    '/DOutputDir=' + str(output), '/DAppVersion=' + args.version,
                    str(ROOT / 'windows/fork-installer.iss')], check=True)
    installer = output / ('RoadRash-3DO-Setup-' + args.version + '.exe')
    digest = hashlib.sha256(installer.read_bytes()).hexdigest()
    (output / 'SHA256SUMS.txt').write_text(digest + '  ' + installer.name + '\n')
    print('Asset-free installer:', installer)
    print('SHA256:', digest)

if __name__ == '__main__':
    main()

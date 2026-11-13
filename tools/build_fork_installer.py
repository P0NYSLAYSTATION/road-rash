"""Package only executables, redistributable libraries, notices and tools."""
import argparse
import ctypes
import os
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
           'libsndfile-COPYING.txt', 'FFmpeg-LICENSE.txt', '7zip-LICENSE.txt', 'Python-LICENSE.txt', 'PyInstaller-LICENSE.txt',
           'FreeType-FTL.txt', 'JPEG-LICENSE.md', 'JPEG-README.ijg',
           'Wine-Wingdings-LGPL.txt', 'wine-fonts/README.md', 'wine-fonts/wingding.sfd']

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', default='build/release')
    parser.add_argument('--version', default='1.1.0')
    parser.add_argument('--test-build', action='store_true')
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
    support = stage / 'support'
    support.mkdir(exist_ok=True)
    tools = {'rr-import.exe': build / 'rr-import.exe',
             'rr-extract.exe': ROOT / 'deps/extractor-bin/rr-extract.exe',
             'ffmpeg.exe': ROOT / 'deps/FFmpeg-n9.0.2/ffmpeg.exe',
             '7z.exe': ROOT / 'deps/7zip/7z.exe', '7z.dll': ROOT / 'deps/7zip/7z.dll'}
    for name, source in tools.items():
        if not source.is_file():raise SystemExit('Run tools/build_installer_helpers.py; missing '+str(source))
        shutil.copy2(source, support / name)
    # The upstream executable embeds disc-derived icon group 1. Releases omit it.
    if os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
        kernel.LoadLibraryExW.restype = ctypes.c_void_p
        kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
        kernel.FindResourceW.restype = ctypes.c_void_p
        kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
        module = kernel.LoadLibraryExW(str(stage / 'rr-game.exe'), None, 2)
        if not module:
            raise SystemExit('Could not audit executable resources')
        try:
            if kernel.FindResourceW(module, 1, 14):
                raise SystemExit('Disc-derived icon is embedded; configure RR_ORIGINAL_APP_ICON=OFF')
        finally:
            kernel.FreeLibrary(module)
    shutil.copy2(ROOT / 'tools/extract_3do_audio.py', stage / 'extract_3do_audio.py')
    licenses = stage / 'licenses'
    licenses.mkdir(exist_ok=True)
    for name in NOTICES:
        (licenses / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / 'third_party' / name, licenses / name)
    info = subprocess.check_output([str(build / 'rr-game.exe'), '--build-info'], text=True)
    json.loads(info)
    (stage / 'BUILD_INFO.json').write_text(info)
    (stage / 'README.txt').write_text('''Road Rash 3DO Audio Port

Select an existing PC installation, original CD drive or ISO. Optionally select
a 3DO BIN/CUE archive (ZIP/7z/RAR); setup extracts and converts audio offline. No game assets or soundfonts are distributed.

Put an SF2, SF3 or DLS bank beside rr-game.exe or in soundfonts/ to override
Windows gm.dls. Saves are in saves/ and are loaded from the in-game menus.
rr-port.ini selects audio=3do or audio=pc and renderer=dx9 or renderer=sdl.
A missing or invalid 3DO bank automatically retains PC audio.

Source and build instructions: https://github.com/P0NYSLAYSTATION/road-rash
Fidelity details: docs/FORK_AUDIO.md in that repository.
BUILD_INFO.json identifies the source revision used for this executable.

This software is based in part on the work of the FreeType Team and the
Independent JPEG Group. It includes unmodified FreeType 2.13.3 (FTL),
libjpeg-turbo 3.1.4.1, and Wine 11.18's LGPL Wingdings fallback font.
The font's FontForge source and license are in licenses/wine-fonts/ and
licenses/Wine-Wingdings-LGPL.txt. Replace third_party/wine-fonts/wingding.ttf
and rebuild to use a modified font.

FluidSynth 2.6.2 and libsndfile 1.2.2 use replaceable dynamic libraries (LGPL).
Licenses and source notices are in licenses/. Offline import tools bundle
Python, PyInstaller, 7-Zip 26.06 and a minimal LGPL FFmpeg 9.0.2 build.
Their sources/build instructions are in tools/build_installer_helpers.py.
https://github.com/ip7z/7zip/tree/26.06
https://github.com/FFmpeg/FFmpeg/tree/n9.0.2 Compatible modified DLLs may
replace the bundled libraries.
https://github.com/FluidSynth/fluidsynth/tree/v2.6.2
https://github.com/libsndfile/libsndfile/tree/1.2.2
''')
    # An allowlist also rejects stale commercial files from an earlier stage.
    allowed = set(RUNTIME + ['README.txt', 'BUILD_INFO.json', 'extract_3do_audio.py'])
    allowed.update('licenses/' + name for name in NOTICES)
    allowed.update('support/' + name for name in tools)
    for file in stage.rglob('*'):
        if file.is_file() and file.relative_to(stage).as_posix() not in allowed:
            raise SystemExit('Unexpected file in asset-free installer stage: ' + str(file))
    output = ROOT / 'artifacts/release'
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str((ROOT / args.iscc).resolve()), '/DSourceDir=' + str(stage),
                    '/DOutputDir=' + str(output), '/DAppVersion=' + args.version, '/DTestBuild=' + ('1' if args.test_build else '0'),
                    str(ROOT / 'windows/fork-installer.iss')], check=True)
    installer = output / ('RoadRash-3DO-Setup-' + args.version + ('-test' if args.test_build else '') + '.exe')
    digest = hashlib.sha256(installer.read_bytes()).hexdigest()
    (output / ('SHA256SUMS-test.txt' if args.test_build else 'SHA256SUMS.txt')).write_text(digest + '  ' + installer.name + '\n')
    print('Asset-free installer:', installer)
    print('SHA256:', digest)

if __name__ == '__main__':
    main()

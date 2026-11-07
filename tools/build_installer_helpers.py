"""Build offline import tools; downloads contain open-source software only."""
import argparse
import ctypes
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
FFMPEG_URL = 'https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n9.0.2.tar.gz'
FFMPEG_SHA256 = '6e374ed621e48faa40639307dff48ba6fe574a509977956d2cce9669b7cc27e9'
ZIP_URL = 'https://github.com/ip7z/7zip/releases/download/26.06/7z2606-x64.msi'
ZIP_SHA256 = '6dc822569e777d9a3ecd5ffcbc22b55600af87e6c5d9810a64e8566cf5a4af9f'

def download(url, destination, expected):
    if not destination.exists():
        with urllib.request.urlopen(url) as source, destination.open('wb') as out:
            shutil.copyfileobj(source, out)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != expected:
        raise RuntimeError('Dependency checksum mismatch: ' + str(destination))

def unpack_archive_reader(msi_path, output):
    """Read the MSI database/cabinet directly; does not install or register 7-Zip."""
    api = ctypes.WinDLL('msi')
    handle = ctypes.c_uint
    api.MsiOpenDatabaseW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.POINTER(handle)]
    api.MsiDatabaseOpenViewW.argtypes = [handle, ctypes.c_wchar_p, ctypes.POINTER(handle)]
    api.MsiRecordReadStream.argtypes = [handle, handle, ctypes.c_void_p, ctypes.POINTER(handle)]
    database, view, record = handle(), handle(), handle()
    if api.MsiOpenDatabaseW(str(msi_path), None, ctypes.byref(database)):
        raise RuntimeError('Cannot read archive-reader package')
    def field(column):
        size = handle(4096)
        text = ctypes.create_unicode_buffer(4096)
        if api.MsiRecordGetStringW(record, column, text, ctypes.byref(size)):
            raise RuntimeError('Invalid package metadata')
        return text.value
    output.mkdir(exist_ok=True)
    try:
        if api.MsiDatabaseOpenViewW(database, 'SELECT `Name`, `Data` FROM `_Streams`', ctypes.byref(view)):
            raise RuntimeError('Cannot open package stream table')
        api.MsiViewExecute(view, 0)
        while api.MsiViewFetch(view, ctypes.byref(record)) == 0:
            if field(1).lower().endswith('.cab'):
                with (output / 'payload.cab').open('wb') as out:
                    while True:
                        count = handle(65536)
                        buffer = ctypes.create_string_buffer(65536)
                        if api.MsiRecordReadStream(record, 2, buffer, ctypes.byref(count)):
                            raise RuntimeError('Cannot read package cabinet')
                        if not count.value:
                            break
                        out.write(buffer.raw[:count.value])
            api.MsiCloseHandle(record)
        api.MsiCloseHandle(view)
        api.MsiDatabaseOpenViewW(database, 'SELECT `File`, `FileName` FROM `File`', ctypes.byref(view))
        api.MsiViewExecute(view, 0)
        names = {}
        while api.MsiViewFetch(view, ctypes.byref(record)) == 0:
            names[field(1)] = field(2).split('|')[-1]
            api.MsiCloseHandle(record)
        api.MsiCloseHandle(view)
    finally:
        api.MsiCloseHandle(database)
    subprocess.run(['expand.exe', '-F:*', str(output / 'payload.cab'), str(output)], check=True)
    for identifier, name in names.items():
        if name.lower() in ('7z.exe', '7z.dll', 'license.txt'):
            source = output / identifier
            if source.exists() and source != output / name:
                shutil.copy2(source, output / name)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--msys-root', type=Path, default=Path('C:/msys64'))
    parser.add_argument('--skip-converter-build', action='store_true')
    args = parser.parse_args()
    deps = ROOT / 'deps'
    deps.mkdir(exist_ok=True)
    source_archive = deps / 'ffmpeg-n9.0.2.tar.gz'
    download(FFMPEG_URL, source_archive, FFMPEG_SHA256)
    source = deps / 'FFmpeg-n9.0.2'
    if not source.exists():
        with tarfile.open(source_archive) as archive:
            archive.extractall(deps, filter='data')
    if not args.skip_converter_build:
        # Only the required audio formats are built. No GPL codecs, networking,
        # external codec libraries, or commercial assets enter the helper.
        folder = source.as_posix()
        if len(folder) > 2 and folder[1] == ':':
            folder = '/' + folder[0].lower() + folder[2:]
        script = "export PATH=/ucrt64/bin:/usr/bin:/bin; cd '" + folder.replace("'", "'\\''") + "'; "
        script += './configure --target-os=mingw32 --arch=x86_64 --disable-autodetect --disable-everything --disable-doc --disable-debug --disable-network --disable-x86asm --disable-ffplay --disable-ffprobe --enable-ffmpeg --enable-protocol=file,pipe --enable-demuxer=threedostr,aiff --enable-decoder=pcm_s8,pcm_u8,pcm_s16be,pcm_s16le,adpcm_ima_ws,sdx2_dpcm --enable-encoder=pcm_s16le --enable-muxer=wav --enable-filter=aresample,anull,aformat,atrim,asetpts --extra-ldflags=-static && make -j6 ffmpeg.exe'
        subprocess.run([str(args.msys_root / 'usr/bin/bash.exe'), '-lc', script], check=True)
    msi = deps / '7z2606-x64.msi'
    download(ZIP_URL, msi, ZIP_SHA256)
    unpack_archive_reader(msi, deps / '7zip')
    venv = deps / 'extractor-venv'
    python = venv / 'Scripts/python.exe'
    if not python.exists():
        subprocess.run([sys.executable, '-m', 'venv', str(venv)], check=True)
    subprocess.run([str(python), '-m', 'pip', 'install', '--disable-pip-version-check', 'pyinstaller==6.16.0'], check=True)
    subprocess.run([str(python), '-m', 'PyInstaller', '--clean', '--noconfirm', '--onefile', '--name', 'rr-extract',
                    '--distpath', str(deps / 'extractor-bin'), '--workpath', str(deps / 'extractor-build'),
                    '--specpath', str(deps), str(ROOT / 'tools/import_3do_archive.py')], check=True)
    notices = ROOT / 'third_party'
    shutil.copy2(source / 'COPYING.LGPLv2.1', notices / 'FFmpeg-LICENSE.txt')
    shutil.copy2(deps / '7zip/License.txt', notices / '7zip-LICENSE.txt')
    python_license = Path(sys.base_prefix) / 'LICENSE.txt'
    shutil.copy2(python_license, notices / 'Python-LICENSE.txt')
    copying = next((venv / 'Lib/site-packages').glob('pyinstaller-*.dist-info/licenses/COPYING.txt'), None)
    if copying is None:
        copying = next((venv / 'Lib/site-packages/PyInstaller').rglob('COPYING.txt'))
    shutil.copy2(copying, notices / 'PyInstaller-LICENSE.txt')
    print('Offline installer helpers ready.')

if __name__ == '__main__':
    main()

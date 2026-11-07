"""Fetch pinned redistributable tools; never downloads commercial game assets."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FLUID_URL = 'https://github.com/FluidSynth/fluidsynth/releases/download/v2.6.2/fluidsynth-v2.6.2-win10-x64-cpp11.zip'
FLUID_HASH = '808295345d43c9c428f3fa87d29fbf97a9926f356df95559ff2689f3bd4072d7'
INNO_URL = 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_1/innosetup-7.1.1-x64.exe'
INNO_HASH = '8a5a6711c3766a6e5fb74f280ba0e3551e719ce94538d901fdbd1f8f9920b3f0'
VCPKG_COMMIT = '6fdd23fa0a9479a9f9a1c48178696f60cf39255d'
BASELINE = 'c6b301c0f1f2c3ded695fc1bd6aca02724032267'

def download(url, target, digest=None):
    if not target.exists():
        print('Downloading', target.name, flush=True)
        with urllib.request.urlopen(url) as response, target.open('wb') as out:
            shutil.copyfileobj(response, out)
    if digest and hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Checksum mismatch: ' + str(target))

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--openssl', action='store_true', help='Build the pinned static OpenSSL dependency')
    p.add_argument('--installer', action='store_true', help='Install Inno Setup privately under deps/')
    a = p.parse_args()
    deps = ROOT / 'deps'
    deps.mkdir(exist_ok=True)
    archive = deps / 'fluidsynth-v2.6.2-win10-x64-cpp11.zip'
    download(FLUID_URL, archive, FLUID_HASH)
    dest = deps / 'fluidsynth'
    dest.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        for entry in z.infolist():
            if not (dest / entry.filename).resolve().is_relative_to(dest.resolve()):
                raise RuntimeError('Unsafe archive entry')
        z.extractall(dest)
    for url, name in [
        ('https://raw.githubusercontent.com/FluidSynth/fluidsynth/v2.6.2/LICENSE', 'fluidsynth-COPYING.txt'),
        ('https://raw.githubusercontent.com/libsndfile/libsndfile/1.2.2/COPYING', 'libsndfile-COPYING.txt')]:
        download(url, deps / name)
    if a.openssl:
        vcpkg = deps / 'vcpkg'
        if not vcpkg.exists():
            subprocess.run(['git', 'clone', '--no-checkout', 'https://github.com/microsoft/vcpkg.git', str(vcpkg)], check=True)
        subprocess.run(['git', '-C', str(vcpkg), 'checkout', VCPKG_COMMIT], check=True)
        subprocess.run(['cmd', '/d', '/c', str(vcpkg / 'bootstrap-vcpkg.bat'), '-disableMetrics'], check=True)
        subprocess.run([str(vcpkg / 'vcpkg.exe'), 'install', '--x-manifest-root=' + str(ROOT),
                        '--x-install-root=' + str(deps / 'vcpkg-installed'), '--triplet=x64-windows-static',
                        '--downloads-root=' + str(deps / 'vcpkg-downloads')], check=True)
    if a.installer:
        setup = deps / 'innosetup-7.1.1-x64.exe'
        download(INNO_URL, setup, INNO_HASH)
        subprocess.run([str(setup), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
                        '/DIR=' + str(deps / 'inno')], check=True)
    print('Windows dependencies ready.')

if __name__ == '__main__':
    main()

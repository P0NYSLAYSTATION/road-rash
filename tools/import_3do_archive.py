"""Offline setup helper: import an owned 3DO BIN/CUE archive into a WAV bank."""
import argparse
import contextlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from extract_3do_audio import extract

NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)

def archive_names(archive, archiver):
    import zipfile
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            return [item.filename for item in source.infolist() if not item.is_dir()]
    result = subprocess.run([str(archiver), 'l', '-slt', '-sccUTF-8', '--', str(archive)],
                            capture_output=True, text=True, encoding='utf-8',
                            errors='replace', creationflags=NO_WINDOW, check=True)
    listing = result.stdout.split('----------', 1)
    if len(listing) != 2:
        raise ValueError('Unsupported or damaged archive; use ZIP or 7z')
    return re.findall(r'^Path = (.+)$', listing[1], re.M)

def extract_entry(archive, name, destination, archiver):
    import zipfile
    # Entry names are never used as filesystem destinations.
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source, source.open(name) as data, destination.open('wb') as out:
            shutil.copyfileobj(data, out)
    else:
        with destination.open('wb') as out:
            result = subprocess.run([str(archiver), 'e', '-so', '-spd', '--', str(archive), name],
                                    stdout=out, stderr=subprocess.PIPE, creationflags=NO_WINDOW)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

def choose_image(archive, names, scratch, archiver):
    bins = [name for name in names if PurePosixPath(name.replace('\\', '/')).suffix.lower() in ('.bin', '.iso')]
    cues = [name for name in names if name.lower().endswith('.cue')]
    if not bins:
        raise ValueError('No 3DO BIN image found in the archive')
    preferred = []
    for number, cue in enumerate(sorted(cues)):
        local = scratch / ('disc-' + str(number) + '.cue')
        extract_entry(archive, cue, local, archiver)
        if local.stat().st_size > 1024 * 1024:
            raise ValueError('Invalid CUE size')
        text = local.read_text(encoding='utf-8-sig', errors='replace')
        current = None
        for line in text.splitlines():
            file = re.match(r'\s*FILE\s+(?:"([^"]+)"|(\S+))\s+', line, re.I)
            if file:
                current = file.group(1) or file.group(2)
            if re.match(r'\s*TRACK\s+\d+\s+MODE1/(2352|2048)', line, re.I) and current:
                full = str(PurePosixPath(cue.replace('\\', '/')).parent / current.replace('\\', '/'))
                for candidate in bins:
                    if candidate.replace('\\', '/').lower() == full.lower():
                        preferred.append(candidate)
    # total-order: the CUE data track is preferred, then archive entry name.
    ordered = list(dict.fromkeys(preferred + sorted(bins)))
    for name in ordered:
        image = scratch / 'disc-image.bin'
        extract_entry(archive, name, image, archiver)
        with image.open('rb') as source:
            header = source.read(4096)
        if header[:7] == b'\x01ZZZZZ\x01' or header[16:23] == b'\x01ZZZZZ\x01':
            return image
    raise ValueError('The archive does not contain a readable 3DO Opera disc image')

def import_archive(archive, output, ffmpeg, archiver):
    archive = archive.resolve(strict=True)
    if output.is_symlink():
        raise ValueError('Cannot replace a linked audio destination')
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output == output.parent or output.name.lower() != '3do_audio':
        raise ValueError('Destination must be the installation 3do_audio directory')
    with tempfile.TemporaryDirectory(prefix='rr-import-', dir=output.parent) as temporary:
        scratch = Path(temporary)
        image = choose_image(archive, archive_names(archive, archiver), scratch, archiver)
        bank = scratch / 'bank'
        extract(image, bank, str(ffmpeg))
        manifest = json.loads((bank / 'manifest.json').read_text())
        if len(manifest['samples']) != 19:
            raise ValueError('Incomplete 3DO race sample bank')
        for number in range(1, 20):
            if not (bank / ('resource%02d.wav' % number)).is_file():
                raise ValueError('Incomplete 3DO race sample bank')
        # Preserve an existing imported bank until the new conversion has passed.
        backup = None
        if output.exists():
            if output.is_symlink():
                raise ValueError('Cannot replace a linked audio destination')
            index = 1
            backup = output.with_name(output.name + '.previous')
            while backup.exists():
                index += 1
                backup = output.with_name(output.name + '.previous-' + str(index))
            assert backup.resolve().parent == output.parent
            output.rename(backup)
        try:
            assert bank.resolve().parent == scratch.resolve()
            bank.rename(output)
        except Exception:
            if backup is not None and not output.exists():
                backup.rename(output)
            raise

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--ffmpeg', type=Path, required=True)
    parser.add_argument('--archiver', type=Path, required=True)
    parser.add_argument('--log', type=Path)
    args = parser.parse_args()
    with contextlib.ExitStack() as stack:
        if args.log:
            log = stack.enter_context(args.log.open('w', encoding='utf-8'))
            stack.enter_context(contextlib.redirect_stdout(log))
            stack.enter_context(contextlib.redirect_stderr(log))
        try:
            import_archive(args.archive, args.output, args.ffmpeg, args.archiver)
            print('3DO audio import completed.')
            return 0
        except Exception:
            traceback.print_exc()
            return 1

if __name__ == '__main__':
    sys.exit(main())

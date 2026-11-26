"""Package a complete static documentation site with reproducible hashes and gzip."""
import gzip
import hashlib
import json
from pathlib import Path
import sys
import tarfile
import zipfile

def harfbuzz_license(ports):
    for license_file in Path(ports).glob('harfbuzz/harfbuzz-*/COPYING'):
        try:
            return license_file.read_bytes()
        except PermissionError:
            pass
    for archive in Path(ports).glob('harfbuzz*.tar.*'):
        with tarfile.open(archive) as source:
            for entry in source:
                root, _, name = entry.name.partition('/')
                if root.startswith('harfbuzz-') and name == 'COPYING' and entry.isfile():
                    return source.extractfile(entry).read()
    raise ValueError('Missing HarfBuzz license in the Emscripten port cache')


def package(site, output, ports):
    site, output = Path(site).resolve(), Path(output).resolve()
    (site / 'licenses/HarfBuzz.txt').write_bytes(harfbuzz_license(ports))
    required = ['index.html', 'sunfleck.js', 'sunfleck.wasm', 'sunfleck.data', 'main.js',
                'host.js', 'renderer.js', 'text-input.js', 'blue-marble.jpg',
                'licenses/OFL-CJK.txt', 'licenses/OFL-Mono.txt', 'licenses/SheenBidi-LICENSE.txt',
                'licenses/Sunfleck-LICENSE.txt', 'licenses/FreeType.txt',
                'licenses/HarfBuzz.txt', 'licenses/Emscripten.txt']
    for name in required:
        if not (site / name).is_file():
            raise ValueError(f'Missing required asset: {name}')
    files = sorted(p for p in site.rglob('*') if p.is_file() and p.suffix != '.gz' and p.name != 'resources.json')
    manifest = []
    for file in files:
        data = file.read_bytes()
        manifest.append({'path': file.relative_to(site).as_posix(), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if file.suffix in ('.html', '.js', '.json', '.wasm', '.data', '.ttf', '.txt'):
            file.with_name(file.name + '.gz').write_bytes(gzip.compress(data, mtime=0))
    (site / 'resources.json').write_text(json.dumps({'version': 1, 'files': manifest}, ensure_ascii=False, indent=2), encoding='utf-8')
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(site.rglob('*')):
            if file.is_file():
                archive.write(file, file.relative_to(site))
    print(f'{len(manifest)} assets; {output.stat().st_size} bytes: {output}')

if __name__ == '__main__':
    package(*sys.argv[1:])

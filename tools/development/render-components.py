"""Render the checked-in Fleck examples with the real Sunfleck native library.

Windows: python tools/development/render-components.py --library out/devtools/sunfleck_native.dll
Uses only Python's standard library; the output is the framework's RGBA surface.
"""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]


class Options(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ('size', 'version', 'renderer', 'flags')]


class Diagnostic(c.Structure):
    _fields_ = [('code', c.c_int), ('line', c.c_uint32), ('column', c.c_uint32), ('message', c.c_char * 128)]


def png(width, height, rgba):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    rows = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def resize(width, height, rgba, maximum=480):
    if width <= maximum:
        return width, height, rgba
    target_width = maximum
    target_height = max(1, round(height * target_width / width))
    output = bytearray(target_width * target_height * 4)
    for y in range(target_height):
        source_y = min(height - 1, y * height // target_height)
        for x in range(target_width):
            source_x = min(width - 1, x * width // target_width)
            source = (source_y * width + source_x) * 4
            destination = (y * target_width + x) * 4
            output[destination:destination + 4] = rgba[source:source + 4]
    return target_width, target_height, bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', required=True)
    parser.add_argument('--example', help='Render one example stem and write separate evidence for that example.')
    args = parser.parse_args()
    library = Path(args.library).resolve()
    api = c.CDLL(str(library))
    api.ui_api_version.restype = c.c_uint32
    if api.ui_api_version() != 3:
        raise RuntimeError('This renderer requires Sunfleck API v3')

    def function(name, arguments, result=c.c_int):
        fn = getattr(api, name)
        fn.argtypes, fn.restype = arguments, result
        return fn

    create = function('ui_app_create', [c.POINTER(Options), c.POINTER(c.c_void_p)])
    load = function('ui_app_load_fleck', [c.c_void_p, c.c_char_p, c.c_size_t, c.POINTER(Diagnostic)])
    theme = function('ui_app_set_builtin_theme', [c.c_void_p, c.c_int, c.c_uint32, c.c_uint32])
    open_window = function('ui_app_open_window', [c.c_void_p])
    poll = function('ui_app_poll', [c.c_void_p, c.c_uint32])
    pixels = function('ui_app_pixels', [c.c_void_p, c.POINTER(c.c_uint32), c.POINTER(c.c_uint32)], c.POINTER(c.c_uint8))
    destroy = function('ui_app_destroy', [c.c_void_p], None)
    schema = function('ui_fleck_get_schema', [c.c_void_p, c.POINTER(c.c_size_t)])

    def check(value, operation):
        if value != 0:
            raise RuntimeError(f'{operation}: Sunfleck status {value}')

    size = c.c_size_t()
    check(schema(None, c.byref(size)), 'schema size')
    buffer = c.create_string_buffer(size.value)
    check(schema(buffer, c.byref(size)), 'schema')
    metadata = json.loads(buffer.value)
    reference = (ROOT / 'guides/fleck-components.md').read_text(encoding='utf-8')
    missing = [item['name'] for item in metadata['elements'] if f"`{item['name']}`" not in reference]
    if missing:
        raise RuntimeError(f'Components missing from the reference: {missing}')

    output = ROOT / 'assets'
    output.mkdir(exist_ok=True)
    evidence = {'renderer': 'Sunfleck Windows headless software / Radix light',
                'librarySha256': hashlib.sha256(library.read_bytes()).hexdigest(),
                'schemaComponents': len(metadata['elements']), 'examples': []}
    sources = sorted((ROOT / 'snippets/fleck').glob('*.fleck'))
    if args.example:
        sources = [source for source in sources if source.stem == args.example]
        if not sources:
            raise ValueError(f'Unknown example: {args.example}')
    for source in sources:
        xml = source.read_bytes()
        app = c.c_void_p()
        check(create(c.byref(Options(c.sizeof(Options), 3, 2, 1)), c.byref(app)), 'create')
        try:
            diagnostic = Diagnostic()
            result = load(app, xml, len(xml), c.byref(diagnostic))
            check(result, f'{source.name}:{diagnostic.line} {diagnostic.message!r}')
            check(theme(app, 2, 0, 0), 'theme')
            check(open_window(app), 'open')
            check(poll(app, 0), 'poll')
            width, height = c.c_uint32(), c.c_uint32()
            data = pixels(app, c.byref(width), c.byref(height))
            if not data or not width.value or not height.value:
                raise RuntimeError(f'No rendered surface for {source}')
            native_width, native_height = width.value, height.value
            rendered = c.string_at(data, native_width * native_height * 4)
            preview_width, preview_height, preview_pixels = resize(native_width, native_height, rendered)
            target = output / f'fleck-{source.stem}-preview.png'
            target.write_bytes(png(preview_width, preview_height, preview_pixels))
            evidence['examples'].append({'source': source.relative_to(ROOT).as_posix(),
                'sourceSha256': hashlib.sha256(xml).hexdigest(), 'image': target.relative_to(ROOT).as_posix(),
                'imageSha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                'nativeWidth': native_width, 'nativeHeight': native_height,
                'width': preview_width, 'height': preview_height})
            print(f'{source.name}: rendered {native_width}x{native_height}, preview {preview_width}x{preview_height}')
        finally:
            destroy(app)
    evidence_name = f'fleck-{sources[0].stem}-evidence.json' if args.example else 'fleck-previews.json'
    (output / evidence_name).write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()

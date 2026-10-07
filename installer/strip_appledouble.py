#!/usr/bin/env python3
"""Removes AppleDouble ("._*") entries from a flat component .pkg.

macOS tags every file created on this machine with a protected 'com.apple.provenance' attribute that cannot be
cleared, and pkgbuild archives such attributes as extra "._name" entries. They are harmless (the installer turns
them back into attributes, not files) but clutter the package. This rewrites the package without them: the
Payload (gzip'd cpio, odc format) and Scripts archives are filtered, the Bom is rebuilt from the filtered file
list (modes, owners, sizes and checksums are kept) and PackageInfo's file count is updated.

Usage: strip_appledouble.py component.pkg      (rewritten in place)
"""
import gzip, math, os, re, shutil, subprocess, sys, tempfile

def entries(data):
    pos = 0
    while pos < len(data):
        hdr = data[pos:pos + 76]
        if hdr[:6] != b'070707':
            raise SystemExit('not an odc cpio archive')
        namesize = int(hdr[59:65], 8); filesize = int(hdr[65:76], 8)
        name = data[pos + 76:pos + 76 + namesize - 1].decode()
        body_at = pos + 76 + namesize
        yield hdr, name, data[body_at:body_at + filesize]
        pos = body_at + filesize
        if name == 'TRAILER!!!':
            return   # anything after the trailer is block padding

def filter_cpio(path):
    """Rewrites a gzip'd cpio file without '._' entries. Returns (kept_files, removed_files, removed_bytes)."""
    with gzip.open(path, 'rb') as f: data = f.read()
    out = bytearray(); kept = removed = removed_bytes = 0
    for hdr, name, body in entries(data):
        base = os.path.basename(name.rstrip('/'))
        if base.startswith('._'):
            removed += 1; removed_bytes += len(body); continue
        out += hdr + name.encode() + b'\0' + body
        if name != 'TRAILER!!!': kept += 1
    with gzip.open(path, 'wb', compresslevel=9) as f: f.write(bytes(out))
    return kept, removed, removed_bytes

def main(pkg):
    pkg = os.path.abspath(pkg)
    work = tempfile.mkdtemp(); exp = os.path.join(work, 'exp')
    subprocess.run(['pkgutil', '--expand', pkg, exp], check=True)
    kept, removed, removed_bytes = filter_cpio(os.path.join(exp, 'Payload'))
    scripts = os.path.join(exp, 'Scripts')
    if os.path.isfile(scripts):
        filter_cpio(scripts)
    elif os.path.isdir(scripts):          # pkgutil --expand unpacks the scripts into a folder
        for folder, _, names in os.walk(scripts):
            for n in names:
                if n.startswith('._'): os.remove(os.path.join(folder, n))
    if removed == 0:
        print('no AppleDouble entries found; package unchanged'); shutil.rmtree(work); return
    # Bom: rebuild from the filtered list
    bom = os.path.join(exp, 'Bom')
    listing = subprocess.run(['lsbom', bom], check=True, capture_output=True, text=True).stdout.splitlines()
    clean = [l for l in listing if not os.path.basename(l.split('\t')[0]).startswith('._')]
    lst = os.path.join(work, 'list.txt'); open(lst, 'w').write('\n'.join(clean) + '\n')
    os.remove(bom); subprocess.run(['mkbom', '-i', lst, bom], check=True)
    # PackageInfo counts
    info = os.path.join(exp, 'PackageInfo'); text = open(info).read()
    m = re.search(r'<payload numberOfFiles="(\d+)" installKBytes="(\d+)"', text)
    if m:
        kb = max(0, int(m.group(2)) - math.ceil(removed_bytes / 1024))
        text = text.replace(m.group(0), f'<payload numberOfFiles="{kept}" installKBytes="{kb}"')
        open(info, 'w').write(text)
    out = os.path.join(work, 'out.pkg')
    subprocess.run(['pkgutil', '--flatten', exp, out], check=True)
    shutil.move(out, pkg); shutil.rmtree(work)
    print(f'removed {removed} AppleDouble entries; {kept} entries remain')

if __name__ == '__main__':
    main(sys.argv[1])

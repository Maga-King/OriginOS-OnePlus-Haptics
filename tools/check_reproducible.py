#!/usr/bin/env python3
import argparse,hashlib,json,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('first');p.add_argument('second');a=p.parse_args()
left=Path(a.first);right=Path(a.second)
names=['mio-vibrator','driver_query','source.zip','waveforms.zip','build-info.json','SHA256SUMS']
names+=[p.name for p in left.glob('OriginOS-OnePlus-Haptics-*.zip')]
for name in names:
    assert (left/name).read_bytes()==(right/name).read_bytes(),name+' not reproducible'
    print('IDENTICAL',name,hashlib.sha256((left/name).read_bytes()).hexdigest())
with zipfile.ZipFile(next(left.glob('OriginOS-OnePlus-Haptics-*.zip'))) as z:
    assert z.read('bin/mio-vibrator')==(left/'mio-vibrator').read_bytes()
    assert 'META-INF/com/google/android/update-binary' in z.namelist()
    assert z.read('META-INF/com/google/android/updater-script').strip()==b'#MAGISK'
    assert all(not n.startswith('/') and '..' not in Path(n).parts for n in z.namelist())
    assert len([n for n in z.namelist() if n.startswith('waves/') and n.endswith('.bin')])==615
print('PASS module layout, bundled source/wave ZIPs, HAL byte identity and two-directory reproducibility')

#!/usr/bin/env python3
import argparse,hashlib,json,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('first');p.add_argument('second');a=p.parse_args()
left=Path(a.first);right=Path(a.second)
version=json.loads((left/'build-info.json').read_text())['version']
module=f'Nyako-OriginOS-Haptics-v{version}.zip'
builtin=f'Nyako-OriginOS-Haptics-Builtin-v{version}.zip'
names=['nyako-vibrator','driver_query','source.zip','waveforms.zip','build-info.json','SHA256SUMS','BUILTIN-SHA256SUMS',module,builtin]
for name in names:
    assert (left/name).read_bytes()==(right/name).read_bytes(),name+' not reproducible'
    print('IDENTICAL',name,hashlib.sha256((left/name).read_bytes()).hexdigest())
with zipfile.ZipFile(left/module) as z:
    assert z.read('bin/nyako-vibrator')==(left/'nyako-vibrator').read_bytes()
    assert 'META-INF/com/google/android/update-binary' in z.namelist()
    assert z.read('META-INF/com/google/android/updater-script').strip()==b'#MAGISK'
    assert all(not n.startswith('/') and '..' not in Path(n).parts for n in z.namelist())
    assert len([n for n in z.namelist() if n.startswith('waves/') and n.endswith('.bin')])==615
with zipfile.ZipFile(left/builtin) as z:
    assert z.read('odm/bin/hw/vendor.oplus.hardware.vibrator-service')==(left/'nyako-vibrator').read_bytes()
    assert not any(n.endswith(('.sh','.py')) or n.startswith('META-INF/') for n in z.namelist())
    assert len([n for n in z.namelist() if n.startswith('odm/etc/nyako-vibrator/waves/') and n.endswith('.bin')])==615
    assert all(not n.startswith('/') and '..' not in Path(n).parts for n in z.namelist())
print('PASS module and manual-builtin layouts, bundled source/wave ZIPs, identical HAL in both packages and two-directory reproducibility')

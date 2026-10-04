"""Package the existing HAL for metamodule mounting; no recompilation."""
from pathlib import Path
import argparse,json,zipfile
from build import archive
ROOT=Path(__file__).resolve().parents[1]
def package_metamodule(module,out,plugin):
    out=Path(out)
    with zipfile.ZipFile(module) as z:
        info=json.loads(z.read('build-info.json'))
        entries=[('module.prop',z.read('module.prop'),False),
                 ('system/odm/bin/hw/vendor.oplus.hardware.vibrator-service',z.read('bin/nyako-vibrator'),True)]
        for name in z.namelist():
            if name.startswith('waves/'):
                entries.append(('system/odm/etc/nyako-vibrator/'+name,z.read(name),False))
        for name in ['build-info.json','scenes.json','source.zip','waveforms.zip','NOTICE.md','README_CN.md']:
            entries.append((name,z.read(name),False))
        for name in z.namelist():
            if name.startswith('META-INF/') or name.startswith('licenses/'):
                entries.append((name,z.read(name),bool(z.getinfo(name).external_attr>>16 & 0o111)))
    entries.append(('system/lib64/libNyako_hook_haptics_concurrency.so',Path(plugin).read_bytes(),False))
    for p in (ROOT/'metamodule').glob('*.sh'):entries.append((p.name,p.read_bytes(),True))
    path=out/f'Nyako-OriginOS-Haptics-Metamodule-v{info["version"]}.zip'
    archive(path,entries);return path
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--module',required=True);p.add_argument('--out',required=True);p.add_argument('--plugin',required=True)
    a=p.parse_args();print(package_metamodule(a.module,a.out,a.plugin))

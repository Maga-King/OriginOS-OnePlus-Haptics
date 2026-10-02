#!/usr/bin/env python3
"""Wrap the released HAL for ODM integration, without recompiling the ELF."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path
from build import archive
ROOT=Path(__file__).resolve().parents[1]
SERVICE='odm/bin/hw/vendor.oplus.hardware.vibrator-service'
RC='odm/etc/init/vibrator-default.rc'
def package_builtin(module,out):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(module) as z:
        info=json.loads(z.read('build-info.json'))
        entries=[(SERVICE,z.read('bin/nyako-vibrator'),True),(RC,(ROOT/'builtin/vibrator-default.rc').read_bytes(),False)]
        entries += [('odm/etc/nyako-vibrator/'+name,z.read(name),False) for name in sorted(z.namelist()) if name.startswith('waves/') and not name.endswith('/')]
        entries += [('odm/etc/nyako-vibrator/'+name,z.read(name),False) for name in ['build-info.json','scenes.json','NOTICE.md','wave_provenance.json']]
        entries += [('odm/etc/nyako-vibrator/'+name,z.read(name),False) for name in z.namelist() if name.startswith('licenses/')]
    paths={}
    for name,data,executable in entries:
        paths[name]=('0755' if executable else '0644','hal_vibrator_default_exec' if name==SERVICE else 'vendor_configs_file')
        parent=Path(name).parent
        while parent.as_posix()!='.':
            p=parent.as_posix();paths.setdefault(p,('0755','vendor_configs_file' if '/etc' in p else 'vendor_file'));parent=parent.parent
    fs=''.join(f'{p} 0 0 {mode}\n' for p,(mode,label) in sorted(paths.items()))
    fc=''.join('/'+re.escape(p)+f' u:object_r:{label}:s0\n' for p,(mode,label) in sorted(paths.items()))
    entries += [('metadata/odm_fs_config.additions',fs.encode(),False),('metadata/odm_file_contexts.additions',fc.encode(),False),
                ('README_CN.md',(ROOT/'docs/BUILTIN.md').read_bytes(),False)]
    report={**info,'module_sha256':hashlib.sha256(Path(module).read_bytes()).hexdigest(),'layout':'odm','hal_unchanged_from_release':True}
    entries.append(('builtin-info.json',(json.dumps(report,indent=2,sort_keys=True)+'\n').encode(),False))
    name=f'Nyako-OriginOS-Haptics-Builtin-v{info["version"]}.zip';archive(out/name,entries)
    (out/'BUILTIN-SHA256SUMS').write_text(hashlib.sha256((out/name).read_bytes()).hexdigest()+'  '+name+'\n',encoding='utf-8',newline='\n')
    print(name,info['hal_sha256'])
    return out/name
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--module',required=True);ap.add_argument('--out',default='out');a=ap.parse_args()
    package_builtin(a.module,a.out)
if __name__=='__main__':main()

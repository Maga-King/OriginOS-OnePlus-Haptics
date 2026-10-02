#!/usr/bin/env python3
"""Build the actual ARM64 HAL and a deterministic, self-contained module."""
import argparse,hashlib,json,os,platform,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NDK_REVISION='27.3.13750724'
VERSION=(ROOT/'VERSION').read_text().strip()

def archive(path,entries):
    path.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(path,'w') as z:
        for name,data,executable in sorted(entries,key=lambda item:item[0]):
            info=zipfile.ZipInfo(name,(2026,1,1,0,0,0))
            info.create_system=3;info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=(0o100755 if executable else 0o100644)<<16
            z.writestr(info,data,compresslevel=9)

def run(args):subprocess.run([str(x) for x in args],check=True,cwd=ROOT)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--ndk',default=os.environ.get('ANDROID_NDK_HOME') or os.environ.get('ANDROID_NDK_ROOT'))
    ap.add_argument('--out',default='out')
    ap.add_argument('--revision',default=os.environ.get('GITHUB_SHA','local'))
    args=ap.parse_args()
    if not args.ndk:ap.error('请用 --ndk 或 ANDROID_NDK_HOME 指定 NDK r27d')
    ndk=Path(args.ndk).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
    properties=(ndk/'source.properties').read_text()
    if 'Pkg.Revision = '+NDK_REVISION not in properties:ap.error('可复现构建使用 NDK '+NDK_REVISION)
    host={'Windows':'windows-x86_64','Linux':'linux-x86_64','Darwin':'darwin-x86_64'}[platform.system()]
    tools=ndk/'toolchains/llvm/prebuilt'/host/'bin';exe='.exe' if platform.system()=='Windows' else ''
    cc=tools/('clang'+exe);cxx=tools/('clang++'+exe)
    flags=['--target=aarch64-linux-android31','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
           '-DMIO_DEMO','-ffile-prefix-map='+str(ROOT)+'=.', '-fdebug-prefix-map='+str(ROOT)+'=.',
           '-fmacro-prefix-map='+str(ROOT)+'=.','-I'+str(ROOT/'src'),'-I'+str(ROOT/'generated/include')]
    obj=out/'rtp_backend.o'
    run([cc,*flags,'-std=c11','-c',ROOT/'src/rtp_backend.c','-o',obj])
    names=['wave_model.cpp','playback_queue.cpp','vibrator_frontend.cpp','phone_backend.cpp','he_model.cpp','he_extension.cpp']
    shared=[ROOT/'src'/name for name in names]+sorted((ROOT/'generated/src').rglob('*.cpp'))
    link=['-lbinder_ndk','-ldl','-static-libstdc++','-Wl,--build-id=none']
    run([cxx,*flags,'-std=c++17',*shared,ROOT/'src/demo_service.cpp',obj,*link,'-o',out/'mio-vibrator'])
    run([cc,*flags,'-std=c11',ROOT/'src/driver_query.c','-Wl,--build-id=none','-o',out/'driver_query'])
    run([cxx,*flags,'-std=c++17',*shared,ROOT/'tests/test_frontend.cpp',obj,*link,'-o',out/'test_frontend'])
    for name in ['mio-vibrator','driver_query','test_frontend']:
        run([tools/('llvm-strip'+exe),'--strip-debug',out/name])
    # Generated AIDL is committed: no Android SDK generator or local ROM dump needed.
    source=[]
    for folder in ['src','tests','tools','module','assets','aidl','generated','licenses','docs','.github','builtin']:
        for p in sorted((ROOT/folder).rglob('*')):
            if p.is_file() and '__pycache__' not in p.parts:
                source.append((p.relative_to(ROOT).as_posix(),p.read_bytes(),p.suffix in ['.sh','.py']))
    for name in ['README.md','NOTICE.md','VERSION','.gitattributes','.gitignore']:
        source.append((name,(ROOT/name).read_bytes(),False))
    archive(out/'source.zip',source)
    entries=[]
    for p in sorted((ROOT/'module').rglob('*')):
        if p.is_file():
            name=p.relative_to(ROOT/'module').as_posix()
            entries.append((name,p.read_bytes(),p.suffix=='.sh' or name.endswith('update-binary')))
    for name in ['mio-vibrator','driver_query']:
        entries.append(('bin/'+name,(out/name).read_bytes(),True))
    with zipfile.ZipFile(ROOT/'assets/waveforms.zip') as z:
        for name in sorted(z.namelist()):
            if not name.startswith('waves/') or '..' in Path(name).parts:raise ValueError('invalid bundled asset path')
            entries.append((name,z.read(name),False))
    entries.extend([('source.zip',(out/'source.zip').read_bytes(),False),
                    ('waveforms.zip',(ROOT/'assets/waveforms.zip').read_bytes(),False),
                    ('README_CN.md',(ROOT/'README.md').read_bytes(),False),
                    ('NOTICE.md',(ROOT/'NOTICE.md').read_bytes(),False),
                    ('wave_provenance.json',(ROOT/'assets/provenance.json').read_bytes(),False),
                    ('scenes.json',(ROOT/'docs/SCENES.json').read_bytes(),False)])
    for p in sorted((ROOT/'licenses').glob('*')):
        if p.is_file():entries.append(('licenses/'+p.name,p.read_bytes(),False))
    for name in ['NOTICE','NOTICE.toolchain']:
        entries.append(('licenses/NDK-'+name,(ndk/name).read_bytes(),False))
    info=dict(version=VERSION,revision=args.revision,ndk=NDK_REVISION,abi='arm64-v8a',api=31,
              supported_ids=765,compatibility_ids=[65,691,3066,3103,26007],
              hal_sha256=hashlib.sha256((out/'mio-vibrator').read_bytes()).hexdigest())
    metadata=(json.dumps(info,ensure_ascii=False,sort_keys=True,indent=2)+'\n').encode()
    (out/'build-info.json').write_bytes(metadata);entries.append(('build-info.json',metadata,False))
    package=out/f'OriginOS-OnePlus-Haptics-v{VERSION}.zip';archive(package,entries)
    shutil.copyfile(ROOT/'assets/waveforms.zip',out/'waveforms.zip')
    release=[package,out/'mio-vibrator',out/'driver_query',out/'source.zip',out/'waveforms.zip',out/'build-info.json']
    (out/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in release),encoding='utf-8',newline='\n')
    print(json.dumps(info,ensure_ascii=False));print('Built:',package)
if __name__=='__main__':main()

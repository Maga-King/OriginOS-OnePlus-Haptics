"""Build our plugin only, using an existing Nyako checkout/core as dependencies."""
from pathlib import Path
import argparse,os,subprocess,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parent
p=argparse.ArgumentParser(__doc__)
for name in ['repo','sdk','java','ndk','core']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
out=ROOT/'out';out.mkdir(exist_ok=True)
work=Path(tempfile.mkdtemp(prefix='build-',dir=out))
api,classes,dex=[work/n for n in ['api','classes','dex']]
for f in [api,classes,dex]:f.mkdir()
exe='.exe' if os.name=='nt' else ''
host={'win32':'windows-x86_64','linux':'linux-x86_64','darwin':'darwin-x86_64'}[sys.platform]
llvm=a.ndk/'toolchains/llvm/prebuilt'/host/'bin'
android=a.sdk/'platforms/android-37.0/android.jar'
if not android.exists():android=a.sdk/'platforms/android-37/android.jar'
d8=a.sdk/'build-tools/37.0.0/lib/d8.jar'
def run(*args):subprocess.run([str(s) for s in args],check=True)
def java(name):return a.java/'bin'/(name+exe)
run(java('javac'),'--release','11','-encoding','UTF-8','-cp',android,'-d',api,
    *sorted((a.repo/'nyako_core/runtime/java/com/nyako/api').glob('*.java')))
run(java('jar'),'cf',work/'api.jar','-C',api,'.')
run(java('javac'),'--release','11','-encoding','UTF-8','-cp',os.pathsep.join(map(str,[android,api])),
    '-d',classes,*sorted((ROOT/'src').rglob('*.java')))
run(java('jar'),'cf',work/'module.jar','-C',classes,'.')
run(java('java'),'-cp',d8,'com.android.tools.r8.D8','--release','--min-api','28',
    '--lib',android,'--classpath',work/'api.jar','--output',dex,work/'module.jar')
so=out/'libNyako_hook_haptics_concurrency.so'
run(llvm/('clang++'+exe),'--target=aarch64-linux-android28','-std=c++17','-O2','-shared','-fPIC',
    '-fvisibility=hidden','-static-libstdc++','-Wl,--exclude-libs,ALL','-Wl,--no-undefined',
    '-Wl,-z,max-page-size=16384','-Wl,-soname,'+so.name,'-I'+str(a.repo/'include'),
    '-DCONCURRENCY_DEX_FILE="'+(dex/'classes.dex').as_posix()+'"',
    ROOT/'native/plugin.cpp',ROOT/'native/payload.S',a.core,'-llog','-ldl','-o',so)
archive=out/'Nyako-Haptics-Concurrency-0.1.0.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    z.write(so,so.name);z.write(ROOT/'haptics_concurrency.conf','haptics_concurrency.conf')
print(archive)

#!/usr/bin/env python3
"""Development tests only; none of these checks run on phone startup."""
import hashlib,json,os,struct,subprocess,tempfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args):subprocess.run([str(x) for x in args],check=True,cwd=ROOT)
with tempfile.TemporaryDirectory(prefix='haptics-tests-') as folder:
    tmp=Path(folder)
    with zipfile.ZipFile(ROOT/'assets/waveforms.zip') as z:z.extractall(tmp)
    flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'src')]
    cases=[('model',['tests/test_wave_model.cpp','src/wave_model.cpp'],True),
           ('he',['tests/test_he_model.cpp','src/he_model.cpp','src/wave_model.cpp'],True),
           ('queue',['tests/test_playback_queue.cpp','src/playback_queue.cpp','src/wave_model.cpp'],False)]
    for name,sources,waves in cases:
        run(['g++',*flags,*[ROOT/p for p in sources],'-pthread','-o',tmp/name])
        run([tmp/name,*([tmp/'waves'] if waves else [])])
    run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-I'+str(ROOT/'src'),ROOT/'tests/test_rtp_backend.c',ROOT/'src/rtp_backend.c','-lm','-o',tmp/'rtp'])
    run([tmp/'rtp'])
    # Compare every approved0.1.8 output against its digest, including IME/AI.
    exporter=tmp/'export.cpp'
    exporter.write_text('''#include "wave_model.h"
#include <fstream>
int main(int n,char**v){if(n!=3)return 2;nyako::WaveModel m(v[1]);std::ofstream f(v[2],std::ios::binary);for(int32_t id:m.supportedEffects()){auto w=m.effect(id);int32_t count=w.size();f.write((char*)&id,4);f.write((char*)&count,4);f.write((char*)w.data(),count);}return !f.good();}
''')
    run(['g++','-std=c++17','-O2','-I'+str(ROOT/'src'),exporter,ROOT/'src/wave_model.cpp','-o',tmp/'export'])
    run([tmp/'export',tmp/'waves',tmp/'scenes.dat'])
    data=(tmp/'scenes.dat').read_bytes();at=0;rows={}
    while at<len(data):
        scene,size=struct.unpack_from('<ii',data,at);at+=8;rows[str(scene)]=hashlib.sha256(data[at:at+size]).hexdigest();at+=size
    baseline=json.loads((ROOT/'tests/scene_baseline.json').read_text())
    assert len(rows)==765 and all(rows[k]==v for k,v in baseline.items())
    print('PASS all762 previous scene outputs unchanged;3explicitcompatibility additions')

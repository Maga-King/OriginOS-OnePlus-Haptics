#!/usr/bin/env python3
"""Publish the just-built files using the Action's scoped GITHUB_TOKEN."""
import json,os,urllib.error,urllib.parse,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
repo=os.environ['GITHUB_REPOSITORY'];token=os.environ['GITHUB_TOKEN']
version=(ROOT/'VERSION').read_text().strip();tag='v'+version
def request(url,method='GET',payload=None,raw=None):
    headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','User-Agent':'OriginOS-OnePlus-Haptics-build','X-GitHub-Api-Version':'2022-11-28'}
    body=None
    if payload is not None:body=json.dumps(payload).encode();headers['Content-Type']='application/json'
    if raw is not None:body=raw;headers['Content-Type']='application/octet-stream'
    with urllib.request.urlopen(urllib.request.Request(url,data=body,method=method,headers=headers),timeout=120) as r:
        data=r.read();return json.loads(data) if data else None
base='https://api.github.com/repos/'+repo
body=(ROOT/'docs/RELEASE_NOTES.md').read_text(encoding='utf-8')
body+='\n\n构建记录：'+os.environ['GITHUB_SERVER_URL']+'/'+repo+'/actions/runs/'+os.environ['GITHUB_RUN_ID']+'\n'
body+='\n源码提交：`'+os.environ['GITHUB_SHA']+'`。\n'
try:release=request(base+'/releases/tags/'+tag)
except urllib.error.HTTPError as e:
    if e.code!=404:raise
    release=request(base+'/releases','POST',dict(tag_name=tag,target_commitish=os.environ['GITHUB_SHA'],name=tag+' · OriginOS 一加震动 HAL',body=body,draft=False,prerelease=False))
else:request(base+'/releases/'+str(release['id']),'PATCH',dict(body=body))
out=ROOT/'out';names=[f'OriginOS-OnePlus-Haptics-v{version}.zip','mio-vibrator','driver_query','source.zip','waveforms.zip','build-info.json','SHA256SUMS']
existing={a['name']:a for a in release.get('assets',[])}
for name in names:
    if name in existing:request(base+'/releases/assets/'+str(existing[name]['id']),'DELETE')
    url=release['upload_url'].split('{')[0]+'?'+urllib.parse.urlencode({'name':name})
    asset=request(url,'POST',raw=(out/name).read_bytes())
    print('Published',asset['name'],asset['size'])
print(release['html_url'])

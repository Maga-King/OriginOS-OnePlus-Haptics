#!/usr/bin/env python3
"""Attach only builtin assets; leave the original tag and module intact."""
import json,os,urllib.request,urllib.parse
from pathlib import Path
def request(url,method='GET',data=None):
    headers={'Authorization':'Bearer '+os.environ['GITHUB_TOKEN'],'Accept':'application/vnd.github+json','User-Agent':'Haptics-Builtin','Content-Type':'application/octet-stream'}
    with urllib.request.urlopen(urllib.request.Request(url,method=method,data=data,headers=headers),timeout=120) as r:
        body=r.read();return json.loads(body) if body else None
base='https://api.github.com/repos/'+os.environ['GITHUB_REPOSITORY']
release=request(base+'/releases/tags/v0.2.0')
assets={a['name']:a for a in release['assets']}
for name in ['OriginOS-OnePlus-Haptics-Builtin-v0.2.0.zip','BUILTIN-SHA256SUMS']:
    if name in assets:request(base+'/releases/assets/'+str(assets[name]['id']),'DELETE')
    url=release['upload_url'].split('{')[0]+'?'+urllib.parse.urlencode({'name':name})
    request(url,'POST',(Path('out')/name).read_bytes());print('Published',name)

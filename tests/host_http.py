#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Integration smoke test: run the real local native host service on a separate port."""
import json, re, selectors, subprocess, sys, time, urllib.request, urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[1]
process=subprocess.Popen([sys.executable,'host/serve.py','--port','0'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
def request(path,data=None,origin=None):
    headers={} if origin is None else {'Origin':origin}
    return urllib.request.urlopen(urllib.request.Request(base+path,data=data,headers=headers),timeout=5).read()
try:
    with selectors.DefaultSelector() as ready:
        ready.register(process.stdout,selectors.EVENT_READ)
        if not ready.select(timeout=30): raise RuntimeError('Host startup timeout')
        startup=process.stdout.readline()
        if not startup: raise RuntimeError(process.communicate())
        address=re.search(rb'http://127\.0\.0\.1:\d+',startup)
        assert address
        base=address.group().decode()
    html=request('/')
    assert b'MC-1' in html
    original=request('/project');assert len(original)==35137 and original[4]==8
    request('/action',json.dumps({'kind':5,'a':63}).encode())
    request('/action',json.dumps({'kind':0,'a':4}).encode())
    request('/action',json.dumps({'kind':2,'a':0,'b':1}).encode())
    edited=request('/project')
    assert edited[92+63*512:92+64*512]!=original[92+63*512:92+64*512], 'Pattern 64 must be editable'
    request('/project',edited);assert request('/project')==edited
    request('/project',original)
    request('/action',json.dumps({'kind':0,'a':9}).encode())
    request('/action',json.dumps({'kind':0,'a':4}).encode())
    request('/action',json.dumps({'kind':1,'a':6,'b':2}).encode())
    request('/action',json.dumps({'kind':1,'a':4,'b':63}).encode())
    request('/action',json.dumps({'kind':1,'a':5,'b':1}).encode())
    song=request('/project');song_offset=len(song)-287
    assert song[song_offset:song_offset+2]==bytes([63,2])
    assert song[song_offset+256:song_offset+259]==bytes([2,0,1])
    request('/project',song);assert request('/project')==song
    request('/project',original)
    assert request('/screen.png').startswith(b'\x89PNG\r\n\x1a\n')
    request('/action',json.dumps({'kind':4,'a':0}).encode())
    request('/action',json.dumps({'kind':1,'a':2,'b':1}).encode())
    popup=request('/screen.png');time.sleep(.55);closed=request('/screen.png')
    assert popup!=closed, 'Stopped transport must expire the preset popup'
    request('/action',json.dumps({'kind':4,'a':1}).encode())
    assert request('/screen.png')==closed, 'Popup expiry restores the same underlying preset screen'
    request('/project',original)
    request('/action',json.dumps({'kind':0,'a':10}).encode())
    pcm=request('/audio');assert len(pcm)==17640 and any(pcm)
    request('/action',json.dumps({'kind':3,'a':3}).encode())
    request('/action',json.dumps({'kind':1,'a':6,'b':5}).encode())
    assert request('/project')!=original
    request('/project',original);assert request('/project')==original
    for path,data,origin,code in [('/project',b'bad',None,400),('/project',bytes(len(original)+1),None,413),('/action',b'{}',None,400),('/action',b'[]',None,400),('/action',b'{}','https://example.com',403)]:
        try: request(path,data,origin)
        except urllib.error.HTTPError as e: assert e.code==code
        else: raise AssertionError('Invalid request accepted')
    print('PASS: native HTTP song editing/save, panel, PNG, stopped popup expiry, PCM, project export/import and invalid request rejection')
finally:
    process.terminate()
    try: process.communicate(timeout=5)
    except subprocess.TimeoutExpired: process.kill();process.communicate()

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Integration smoke test: run the real local native host service on a separate port."""
import json, socket, subprocess, sys, time, urllib.request, urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with socket.socket() as s:
    s.bind(('127.0.0.1',0));port=s.getsockname()[1]
process=subprocess.Popen([sys.executable,'host/serve.py','--port',str(port)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
base=f'http://127.0.0.1:{port}'
def request(path,data=None,origin=None):
    headers={} if origin is None else {'Origin':origin}
    return urllib.request.urlopen(urllib.request.Request(base+path,data=data,headers=headers),timeout=5).read()
try:
    for _ in range(100):
        try: html=request('/');break
        except (OSError,urllib.error.URLError):
            if process.poll() is not None: raise RuntimeError(process.communicate())
            time.sleep(.05)
    else: raise RuntimeError('Host startup timeout')
    assert b'MC-1' in html
    original=request('/project');assert len(original)==2156
    assert request('/screen.png').startswith(b'\x89PNG\r\n\x1a\n')
    request('/action',json.dumps({'kind':0,'a':10}).encode())
    pcm=request('/audio');assert len(pcm)==17640 and any(pcm)
    request('/action',json.dumps({'kind':3,'a':3}).encode())
    request('/action',json.dumps({'kind':1,'a':6,'b':5}).encode())
    assert request('/project')!=original
    request('/project',original);assert request('/project')==original
    for path,data,origin,code in [('/project',b'bad',None,400),('/action',b'{}',None,400),('/action',b'[]',None,400),('/action',b'{}','https://example.com',403)]:
        try: request(path,data,origin)
        except urllib.error.HTTPError as e: assert e.code==code
        else: raise AssertionError('Invalid request accepted')
    print('PASS: native HTTP panel, PNG, PCM, project export/import and invalid request rejection')
finally:
    process.terminate()
    try: process.communicate(timeout=5)
    except subprocess.TimeoutExpired: process.kill();process.communicate()

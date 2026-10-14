#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render/check actual C output; measurements are host evidence, never listening claims."""
import array, hashlib, json, math, subprocess, sys, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
subprocess.run(['make','all'],cwd=root,check=True)
exe=root/'build/mc1-host'
subprocess.run([str(exe),'render',str(root/'build/demo.wav'),'16'],check=True)
files=[root/'build/demo.wav']
for i in range(len(json.loads((root/'presets.json').read_text()))):
    p=root/f'build/preset-{i:02d}.wav';subprocess.run([str(exe),'preset',str(i),str(p)],check=True);files.append(p)
results=[]
for p in files:
    with wave.open(str(p)) as w:
        assert w.getframerate()==44100 and w.getnchannels()==2 and w.getsampwidth()==2
        raw=w.readframes(w.getnframes()); samples=array.array('h',raw)
        if sys.byteorder!='little': samples.byteswap()
        peak=max(map(abs,samples)); rms=math.sqrt(sum(v*v for v in samples)/len(samples));mean=sum(samples)/len(samples)
        assert peak>100 and rms>10 and peak<32767, (p,peak,rms)
        assert abs(mean)<100, (p,mean)
        if p.name!='demo.wav': assert max(map(abs,samples[-4410*2:]))==0, 'Release must settle'
        results.append({'file':p.name,'frames':len(samples)//2,'peak':peak,'rms':round(rms,2),'dc_mean':round(mean,3),'sha256_pcm':hashlib.sha256(raw).hexdigest()})
report={'source':'actual shared C core, host-rendered; not real-device audio','renders':results}
(root/'build/audio-report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: 49 audio renders; non-silent, no saturation, bounded DC, release silence')

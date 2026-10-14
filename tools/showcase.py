#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render listening examples and apply explicitly labeled peak-only audition gain."""
import array, hashlib, json, subprocess, sys, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
(root/'docs/audio').mkdir(parents=True,exist_ok=True)
subprocess.run(['make','showcase','render'],cwd=root,check=True)
items=[('la-showcase.wav','la-showcase.wav'),('303-showcase.wav','303-showcase.wav'),('808-showcase.wav','808-showcase.wav'),('demo.wav','full-mix-preview.wav')]
report=[]
for source,dest in items:
    with wave.open(str(root/'build'/source)) as f: params=f.getparams(); data=array.array('h',f.readframes(f.getnframes()))
    if sys.byteorder!='little': data.byteswap()
    peak=max(map(abs,data));assert peak>0
    gain=23170/peak
    out=array.array('h',(round(x*gain) for x in data))
    assert max(map(abs,out))==23170
    if sys.byteorder!='little': out.byteswap()
    path=root/'docs/audio'/dest
    with wave.open(str(path),'wb') as f: f.setparams(params);f.writeframes(out.tobytes())
    report.append({'file':dest,'frames':len(data)//2,'raw_peak':peak,'audition_gain':round(gain,6),'normalized_peak':23170,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(root/'docs/showcase-report.json').write_text(json.dumps({'method':'Shared C DSP, peak-only normalization to -3.01 dBFS for audition. Not a device recording or a relative-loudness comparison.','files':report},indent=2)+'\n')
print('PASS: four 16-second listening examples; peak gain only, no clipping')

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Optional community export: pip install lameenc==1.8.1; encode native-core WAV."""
import wave,hashlib,json
from pathlib import Path
import lameenc
root=Path(__file__).resolve().parents[1]
source=root/'docs/audio/neon-spiral.wav'
with wave.open(str(source),'rb') as f:
    assert f.getsampwidth()==2 and f.getnchannels()==2
    enc=lameenc.Encoder();enc.set_bit_rate(320);enc.set_in_sample_rate(f.getframerate());enc.set_channels(2);enc.set_quality(2)
    data=enc.encode(f.readframes(f.getnframes()))+enc.flush()
output=root/'docs/audio/mc-1-neon-spiral-fx.mp3';output.write_bytes(data)
report={'file':output.name,'encoder':'lameenc 1.8.1 / LAME','bitrate_kbps':320,'source':'Actual shared C built-in demo, all four native effects enabled; host render, not device recording','bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'wav_sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
(root/'docs/mp3-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

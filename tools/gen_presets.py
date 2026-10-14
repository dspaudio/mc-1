#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Keep factory source, host picker and README bank in sync with original presets."""
import json,re
from pathlib import Path
r=Path(__file__).resolve().parents[1];rows=json.loads((r/'presets.json').read_text());assert len(rows)==48
assert len({p['name'] for p in rows})==len(rows)
for p in rows:
 assert len(p['patch'])==12 and all(0<=v<=127 for v in p['patch'])
 assert p['patch'][0]<2 and p['patch'][1]<3 and p['patch'][2]<12
p=r/'src/factory.c';s=p.read_text();start=s.index('/* Generated from presets.json') if '/* Generated from presets.json' in s else s.index('const char *const');end=s.index('static void clear_bytes')
s=s[:start]+'/* Generated from presets.json by tools/gen_presets.py. */\nconst char *const mc_preset_names[MC_PRESETS] = {\n'+''.join('    '+json.dumps(p['name'])+',\n' for p in rows)+'};\n/* engine,wave,PCM,mix,cutoff,resonance,A,D,S,R,glide,level */\nconst McPatch mc_presets[MC_PRESETS] = {\n'+''.join('    {'+','.join(map(str,p['patch']))+'},\n' for p in rows)+'};\n'+s[end:];p.write_text(s)
p=r/'host/index.html';s=p.read_text();s=re.sub(r'const names=\[.*?\];', 'const names='+json.dumps([p['name'] for p in rows])+';',s);p.write_text(s)
bank='## Complete factory preset list\n\n48 original melodic presets: 36 LA-inspired and 12 303-inspired, plus 16 synthesized drum sounds. These are MC-1 presets, not Roland patch data.\n\n| # | Sound engine | Preset name | Family |\n|---|---|---|---|\n'
for i,p in enumerate(rows):bank+=f"| {i+1:02} | {'303-inspired' if p['patch'][0] else 'LA-inspired'} | {p['name']} | {p['family']} |\n"
bank+='\n| Drum lane | Sound engine | Preset name |\n|---|---|---|\n'
for i,n in enumerate(['KICK','SNARE','CLOSED HAT','OPEN HAT','CLAP','LOW TOM','KICK 2','RIMSHOT','SNARE 2','HIGH TOM','CRASH','RIDE','SHAKER','COWBELL','CLAVES','MID TOM']):bank+=f'| {i+1:02} | 808-inspired synthesis | {n} |\n'
p=r/'README.md';s=p.read_text();start='<!-- BEGIN PRESETS -->';end='<!-- END PRESETS -->'
if start in s:s=s[:s.index(start)]+start+'\n'+bank+end+s[s.index(end)+len(end):]
else:s+='\n'+start+'\n'+bank+end+'\n'
p.write_text(s)
(r/'docs/PRESETS.md').write_text(bank)

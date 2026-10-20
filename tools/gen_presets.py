#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Keep factory source, host picker and README bank in sync with original presets."""
import json,re
from pathlib import Path
from typing import Final
categories: Final = [('Bass',12),('Acid',16),('Lead',12),('Sequence',10),('Keys',10),('Organ',8),('Brass/Reed',8),('Strings',6),('Pad',12),('Pluck',10),('Mallet/Bell',8),('Voice',6),('Hit/Percussion',5),('Texture',5)]
r=Path(__file__).resolve().parents[1];rows=json.loads((r/'presets.json').read_text());assert len(rows)==128
assert len({p['name'] for p in rows})==len(rows)
assert len({tuple(p['patch']) for p in rows})==len(rows)
assert [p['category'] for p in rows]==[category for category,count in categories for _ in range(count)]
assert all(len(category)<=15 for category,count in categories)
enum_ids=['MC_PRESET_'+re.sub(r'[^A-Z0-9]+','_',p['name']).strip('_') for p in rows]
assert len(set(enum_ids))==len(rows)
assert all(len(p['name'])<=18 for p in rows)
assert sum(p['source']=='recorded PCM' for p in rows)==112
assert sum(p['patch'][0]==0 for p in rows)==112
assert sum(p['patch'][0]==1 for p in rows)==16
samples=json.loads((r/'assets/recorded/manifest.json').read_text())['samples']
if len(samples)==16: samples=samples+[{'id':16,'name':'Nylon guitar'}]
assert [sample['id'] for sample in samples]==list(range(17))
assert [sample['name'] for sample in samples]==['Piano','Organ','Marimba','Glockenspiel','Xylophone','Harp','Solo violin','Viola section','Cello section','Contrabass','Flute','Oboe','Clarinet','Bassoon','Trumpet','French horn','Nylon guitar']
for p in rows:
 assert len(p['patch'])==12 and all(0<=v<=127 for v in p['patch'])
 assert p['patch'][0]<2 and p['patch'][1]<3 and p['patch'][2]<41
 recorded=p['patch'][0]==0
 assert p['source']==('recorded PCM' if recorded else 'original synthesis')
 if recorded:
  assert 24<=p['patch'][2]<=40
  assert p['sample']==samples[p['patch'][2]-24]['name']
  assert 0<=p['patch'][5]<=8
p=r/'src/factory.c';s=p.read_text();start=s.index('/* Generated from presets.json') if '/* Generated from presets.json' in s else s.index('const char *const');end=s.index('static void clear_bytes')
s=s[:start]+'/* Generated from presets.json by tools/gen_presets.py. */\nconst char *const mc_preset_names[MC_PRESETS] = {\n'+''.join('    '+json.dumps(p['name'])+',\n' for p in rows)+'};\nconst char *const mc_preset_categories[MC_PRESETS] = {\n'+''.join('    '+json.dumps(p['category'])+',\n' for p in rows)+'};\n/* engine,wave,PCM,mix,cutoff,resonance,A,D,S,R,glide,level */\nconst McPatch mc_presets[MC_PRESETS] = {\n'+''.join('    {'+','.join(map(str,p['patch']))+'},\n' for p in rows)+'};\n'+s[end:];p.write_text(s)
(r/'src/presets.h').write_text('/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 dspaudio */\n/* Generated from presets.json by tools/gen_presets.py. */\n#ifndef MC_PRESETS_H\n#define MC_PRESETS_H\nenum {\n'+''.join(f'    {name} = {i},\n' for i,name in enumerate(enum_ids))+'};\n#endif\n')
p=r/'host/index.html';s=p.read_text();s=re.sub(r'const names=\[.*?\];', 'const names='+json.dumps([p['name'] for p in rows])+';',s);p.write_text(s)
bank='## Complete factory preset list\n\n128 melodic presets: 112 source-backed LA hybrid PCM presets from 17 real instrument families and 16 original 303-inspired Acid presets, plus eight original synthesized drum kits with 16 lanes each. These are MC-1 presets, not Roland patch data.\n\nThe [official MC-505 patch listing (MC505PTC.pdf)](https://cdn.roland.com/assets/media/pdf/MC505PTC.pdf) informed broad sound families such as acid sequences, house keys and organs, voice-like tones and percussion hits. MC-1 uses original patch names and parameters; no Roland patches, ROMs or samples are copied.\n\nAll 128 names, symbolic IDs and category positions are retained. All 112 LA patch definitions are replaced, including the 48 previously mathematical synthesis patches; the 64 previously recorded presets keep their intended instrument families. The 16 Acid definitions and their source metadata, and all eight drum kits, are unchanged. Earlier LA renders are historical audio, not bit-identical examples of this bank.\n\nEach LA sound uses a real recorded attack and two same-phase spectral sustain frames from its source family. PCM mix 127 retains the full recorded attack; the hybrid engine does not blend generic saw or square oscillators at any mix. Plucked sources use their natural per-source decay; bowed strings, organ and winds retain held sustain. Natural, one-pole soft and two-pole rounded attack colours preserve each source while distinguishing felt keys, rounded basses, breath-led winds and clear percussion. Attack contribution and ADSR articulation are tuned per preset; natural pluck decay remains independent. Held-frame evolution is level-matched instead of adding an accent or fade. Approved representative presets remain fixed while the other 106 presets are audited by category. The PCM data budget remains at most 390,144 bytes globally; source generation and engine validation establish the actual allocation.\n\nThe 17 source families are Piano, Organ, Marimba, Glockenspiel, Xylophone, Harp, Solo violin, Viola section, Cello section, Contrabass, Flute, Oboe, Clarinet, Bassoon, Trumpet, French horn and Nylon guitar (PCM IDs 24–40 in that order). Voice presets are processed woodwinds, not human vocal recordings. The names describe register and performance intent; they do not add transpose parameters, new instruments or General MIDI protocol support. These compact source-derived sounds are not full multisampled instruments, and this table is not a listening-validation claim.\n\nThe Source column identifies the actual recorded instrument. See the [recording manifest](assets/hybrid/manifest.json) for source files and processing, and the [licensing and provenance](NOTICE.md).\n\n### Category index\n\n| Category | Presets | Count |\n|---|---|---|\n'
offset=0
for category,count in categories:
 bank+=f'| {category} | {offset+1:03}–{offset+count:03} | {count} |\n';offset+=count
bank+='\n| # | Category | Sound engine | Preset name | Family | Source |\n|---|---|---|---|---|---|\n'
for i,p in enumerate(rows):
 source=p['source']+(f": {p['sample']}" if p['patch'][2]>=24 else '')
 bank+=f"| {i+1:03} | {p['category']} | {'303-inspired' if p['patch'][0] else 'LA-inspired'} | {p['name']} | {p['family']} | {source} |\n"
kit_names=re.findall(r'"([^"]+)"',(r/'src/engine.c').read_text().split('mc_kit_names[MC_KITS]={',1)[1].split('};',1)[0]);assert len(kit_names)==8
kit_descriptions=[
 'Original default kit: deep kicks, noise snares, hats, cymbals and pitched percussion.',
 'Higher kick tuning, stronger pitch sweep and more noise in the snares for club grooves.',
 'Higher tuning and shorter decays for compact, tight drum patterns.',
 'Lower tuning, gentler sweeps and reduced snare noise for a softer vintage character.',
 'Longer kick, open-hat and clap decays for house grooves.',
 'Bright tuning, pronounced sweeps and noisy snares for breakbeat and jungle patterns.',
 'Low tuning, long decays and strong pitch sweeps for heavy metallic textures.',
 'High tom and percussion tuning with shorter clicks for electro and Latin patterns.',
]
bank+='\n### Drum kits\n\nEach kit keeps the same 16 lane roles below and supplies its own original pitch, noise, sweep and decay recipe. Kits share six drum voices; 128 lane variations do not imply 128 simultaneous voices.\n\n| # | Kit name | Character |\n|---|---|---|\n'
for i,(name,description) in enumerate(zip(kit_names,kit_descriptions)):bank+=f'| {i+1:02} | {name} | {description} |\n'
bank+='\n[Listen to all eight kits](docs/audio/kit-showcase.wav): 64 seconds in the table order, eight seconds per kit. Each segment plays all 16 lanes during the first four seconds, then a three-second groove and one second of tail.\n'
bank+='\n| Drum lane | Sound engine | Preset name |\n|---|---|---|\n'
for i,n in enumerate(['KICK','SNARE','CLOSED HAT','OPEN HAT','CLAP','LOW TOM','KICK 2','RIMSHOT','SNARE 2','HIGH TOM','CRASH','RIDE','SHAKER','COWBELL','CLAVES','MID TOM']):bank+=f'| {i+1:02} | Original drum synthesis | {n} |\n'
p=r/'README.md';s=p.read_text();start='<!-- BEGIN PRESETS -->';end='<!-- END PRESETS -->'
if start in s:s=s[:s.index(start)]+start+'\n'+bank+end+s[s.index(end)+len(end):]
else:s+='\n'+start+'\n'+bank+end+'\n'
p.write_text(s)
(r/'docs/PRESETS.md').write_text(bank.replace('(docs/audio/kit-showcase.wav)','(audio/kit-showcase.wav)').replace('(assets/hybrid/manifest.json)','(../assets/hybrid/manifest.json)').replace('(NOTICE.md)','(../NOTICE.md)'))

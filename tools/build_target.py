#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 dspaudio
"""Compile a raw experimental app, never flash or alter attached hardware."""
import json, os, platform, re, subprocess, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
out=root/'build';out.mkdir(exist_ok=True)
tc=Path(os.environ.get('JIELI_TOOLCHAIN',str(Path.home()/'.jieli/toolchain'))).resolve()
docker=platform.system()!='Linux' or platform.machine() not in ('x86_64','AMD64')
probe=os.environ.get('MC_PERF_PROBE','1')
if probe not in ('0','1'): raise SystemExit('MC_PERF_PROBE must be 0 or 1.')
pulse=os.environ.get('MC_PLAY_LED_PULSE','1')
if pulse not in ('0','1'): raise SystemExit('MC_PLAY_LED_PULSE must be 0 or 1.')
dual=os.environ.get('MC_DUAL_CORE','1')
profile=os.environ.get('MC_RENDER_PROFILE','1')
trace=os.environ.get('MC_CPU1_TRACE','0')
if dual not in ('0','1') or profile not in ('0','1'):
    raise SystemExit('MC_DUAL_CORE and MC_RENDER_PROFILE must be 0 or 1.')
if trace not in ('0','1'): raise SystemExit('MC_CPU1_TRACE must be 0 or 1.')
voices=os.environ.get('MC_VOICES','8')
if not re.fullmatch(r'[0-9]+',voices) or not 8<=int(voices)<=32:
    raise SystemExit('MC_VOICES must be an integer from 8 to 32.')
worker_voices=os.environ.get('MC_WORKER_VOICES','3')
if not re.fullmatch(r'[0-9]+',worker_voices) or not 1<=int(worker_voices)<int(voices):
    raise SystemExit('MC_WORKER_VOICES must be an integer from 1 to MC_VOICES-1.')
voice_probe=os.environ.get('MC_VOICE_PROBE','0')
if voice_probe not in ('0','1'): raise SystemExit('MC_VOICE_PROBE must be 0 or 1.')
if int(voices)!=8 or int(worker_voices)!=3:
    raise SystemExit('Target builds are pinned to the hardware-validated 8 melodic / 3 worker voices.')
if os.environ.get('MC_APP_OPTIMIZATION','-Os')!='-Os':
    raise SystemExit('Whole-app -O2 failed hardware boot. Keep the HAL at -Os; use MC_DSP_OPTIMIZATION only.')
dsp_optimization=os.environ.get('MC_DSP_OPTIMIZATION','-O2')
if dsp_optimization not in ('-Os','-O2'): raise SystemExit('MC_DSP_OPTIMIZATION must be -Os or -O2.')
if not (tc/'pi32v2/bin/clang').exists(): raise SystemExit('Set JIELI_TOOLCHAIN to the official JieLi Linux toolchain. No target image was built.')
voice_flags=['-DMC_VOICES='+str(int(voices)),'-DMC_WORKER_VOICES='+str(int(worker_voices)),
             '-DMC_VOICE_PROBE='+voice_probe]
app_flags=voice_flags+(['-DMC_PERF_PROBE=1'] if probe=='1' else [])+['-DMC_PLAY_LED_PULSE='+pulse]
app_flags+=['-DMC_DUAL_WL82=1','-DMC_DUAL_CORE='+dual,'-DMC_RENDER_PROFILE='+profile,'-DMC_CPU1_TRACE='+trace]
if dsp_optimization=='-O2': app_flags+=['-DMC_DSP_SPLIT=1']
def run(tool,*args):
    command=[str(tc/tool),*map(str,args)]
    if docker:
        command=['docker','run','--rm','--platform','linux/amd64','--network','none','-v',f'{root}:/work','-v',f'{tc}:/opt/jieli:ro','-w','/work','debian:bookworm-slim',f'/opt/jieli/{tool}',*map(str,args)]
    return subprocess.check_output(command,text=True,stderr=subprocess.STDOUT,timeout=60)
try:
    version=run('pi32v2/bin/clang','--version')
    for source,name in [('firmware/crt0.S','crt0'),('firmware/hal/fm1_vec.S','vec'),('firmware/hal/fm1_isr.S','isr'),('firmware/app.c','app'),('firmware/update/ota_flash.c','ota_flash')]:
        print(run('pi32v2/bin/clang','-target','pi32v2','-Os','-ffunction-sections','-fno-builtin','-Wall','-Werror','-Wno-unused-function',*(app_flags if name=='app' else []),'-Ifirmware/hal','-Isrc','-c',source,'-o',f'build/{name}.o'),end='')
    objects=['build/crt0.o','build/vec.o','build/isr.o','build/app.o','build/ota_flash.o']
    if dual=='1':
        print(run('pi32v2/bin/clang','-target','pi32v2','-Os','-c','firmware/hal/fm1_cpu1.S','-o','build/cpu1.o'),end='')
        objects.append('build/cpu1.o')
    if dsp_optimization=='-O2':
        for name in ('engine','effects'):
            obj=f'build/dsp_{name}.o'
            print(run('pi32v2/bin/clang','-target','pi32v2','-O2','-ffunction-sections','-fno-builtin','-Wall','-Werror','-DMC_DUAL_WL82=1',*voice_flags,*(['-DMC_PERF_PROBE=1'] if probe=='1' else []),'-Isrc','-c',f'src/{name}.c','-o',obj),end='')
            objects.append(obj)
    print(run('pi32v2/bin/ld','-T','firmware/app.ld',*objects,'-o','build/mc-1.elf'),end='')
    symbols=run('common/bin/objdump','-t','build/mc-1.elf')
    dis=run('common/bin/objdump','-d','build/mc-1.elf')
    ram=run('common/bin/objdump','-d','-j','.ram_text','build/mc-1.elf')
    (out/'mc-1.dis').write_text(dis);(out/'mc-1.symbols').write_text(symbols)
    def sym(n):
        m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(n)+r'$',symbols,re.M)
        if not m: raise RuntimeError('Missing symbol '+n)
        return int(m[1],16)
    image=bytearray()
    for section,load in [('.text',0x02000120),('.ram_text',sym('_rt_load')),('.c1_text',sym('_c1_load')),('.data',sym('_data_load'))]:
        if section=='.c1_text' and sym('_c1_end')==sym('_c1_start'): continue
        dest=f'build/{section[1:]}.bin'
        run('common/bin/objcopy','-O','binary','-j',section,'build/mc-1.elf',dest)
        data=Path(dest).read_bytes();off=load-0x02000120
        if not data: continue
        if off<len(image) or off%4: raise RuntimeError('Invalid load layout')
        image.extend(b'\xff'*(off-len(image)));image.extend(data)
    image.extend(b'\xff'*(-len(image)%4))
    assert sym('_start')==0x02000120 and image[:4]==bytes.fromhex('04818000'), 'Invalid entry'
    assert len(image)<=0x8dfbc, 'Flash app slot overflow'
    assert not re.search(r'\bcall\b',ram), 'Flash-off RAM code contains calls'
    used=sym('_bss_end')-0x01c08000; pool=sym('_pool_end')-sym('_pool_start')
    assert used<=98304 and pool<=0x54000-8192, 'RAM headroom violation'
    assert sym('_rt_end')-sym('_rt_start')<=0x6000, 'RAM code overflow'
    if dual=='1':
        idle=run('common/bin/objdump','-d','-j','.c1_text','build/mc-1.elf')
        calls=[line for line in idle.splitlines() if re.search(r'\bcall\b',line)]
        assert len(calls)==2 and all(re.search(r'\bcall r\d+\b',line) for line in calls), \
            'CPU1 RAM loop must use only its two explicit indirect XIP calls'
        assert sym('_c1_start')>=sym('_rt_end') and sym('_c1_end')<=0x01c06000
    (out/'mc-1.app.bin').write_bytes(image)
    report={'status':'pi32v2 compiled; NOT hardware tested; raw app, NOT an installer','app_bytes':len(image),'app_limit':0x8dfbc,'data_bss_bytes':used,'data_bss_limit':98304,'pool_bytes':pool,'pool_limit':0x54000,'ram_text_bytes':sym('_rt_end')-sym('_rt_start'),'play_led_pulse':pulse=='1','sha256':hashlib.sha256(image).hexdigest(),'compiler':re.sub(r'(?m)^InstalledDir: .+$','InstalledDir: <toolchain>/pi32v2/bin',version.strip())}
    report['app_optimization']='-Os'
    report['dsp_optimization']=dsp_optimization
    report['dual_core']=dual=='1'
    report['render_profile']=profile=='1'
    report['cpu1_trace']=trace=='1'
    report['perf_probe']=probe=='1'
    report['melody_voices']=int(voices)
    report['worker_voices']=int(worker_voices)
    report['voice_probe']=voice_probe=='1'
    report['cpu1_ram_text_bytes']=sym('_c1_end')-sym('_c1_start')
    report['cpu1_user_stack_bytes']=4096 if dual=='1' else 0
    report['cpu1_supervisor_stack_bytes']=512 if dual=='1' else 0
    report['cpu1_stack_guard_bytes']=96 if dual=='1' else 0
    report['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
    report['source_dirty']=bool(subprocess.check_output(['git','status','--porcelain'],text=True).strip())
    inputs=sorted(p for folder in ('src','firmware') for p in (root/folder).rglob('*')
                  if p.is_file() and p.suffix in ('.c','.h','.S','.ld'))
    inputs.append(root/'tools/build_target.py')
    source_hash=hashlib.sha256()
    for path in inputs:
        source_hash.update(str(path.relative_to(root)).encode()+b'\0'+path.read_bytes()+b'\0')
    report['source_input_sha256']=source_hash.hexdigest()
    (out/'target-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
except subprocess.CalledProcessError as e:
    print(e.output);raise SystemExit(e.returncode)

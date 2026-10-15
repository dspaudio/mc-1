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
if not (tc/'pi32v2/bin/clang').exists(): raise SystemExit('Set JIELI_TOOLCHAIN to the official JieLi Linux toolchain. No target image was built.')
docker=platform.system()!='Linux' or platform.machine() not in ('x86_64','AMD64')
probe=os.environ.get('MC_PERF_PROBE','0')
if probe not in ('0','1'): raise SystemExit('MC_PERF_PROBE must be 0 or 1.')
probe_flags=['-DMC_PERF_PROBE=1'] if probe=='1' else []
def run(tool,*args):
    command=[str(tc/tool),*map(str,args)]
    if docker:
        command=['docker','run','--rm','--platform','linux/amd64','--network','none','-v',f'{root}:/work','-v',f'{tc}:/opt/jieli:ro','-w','/work','debian:bookworm-slim',f'/opt/jieli/{tool}',*map(str,args)]
    return subprocess.check_output(command,text=True,stderr=subprocess.STDOUT,timeout=60)
try:
    version=run('pi32v2/bin/clang','--version')
    for source,name in [('firmware/crt0.S','crt0'),('firmware/hal/fm1_vec.S','vec'),('firmware/hal/fm1_isr.S','isr'),('firmware/app.c','app'),('firmware/update/ota_flash.c','ota_flash')]:
        print(run('pi32v2/bin/clang','-target','pi32v2','-Os','-ffunction-sections','-fno-builtin','-Wall','-Werror','-Wno-unused-function',*(probe_flags if name=='app' else []),'-Ifirmware/hal','-Isrc','-c',source,'-o',f'build/{name}.o'),end='')
    print(run('pi32v2/bin/ld','-T','firmware/app.ld','build/crt0.o','build/vec.o','build/isr.o','build/app.o','build/ota_flash.o','-o','build/mc-1.elf'),end='')
    symbols=run('common/bin/objdump','-t','build/mc-1.elf')
    dis=run('common/bin/objdump','-d','build/mc-1.elf')
    ram=run('common/bin/objdump','-d','-j','.ram_text','build/mc-1.elf')
    (out/'mc-1.dis').write_text(dis);(out/'mc-1.symbols').write_text(symbols)
    def sym(n):
        m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(n)+r'$',symbols,re.M)
        if not m: raise RuntimeError('Missing symbol '+n)
        return int(m[1],16)
    image=bytearray()
    for section,load in [('.text',0x02000120),('.ram_text',sym('_rt_load')),('.data',sym('_data_load'))]:
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
    (out/'mc-1.app.bin').write_bytes(image)
    report={'status':'pi32v2 compiled; NOT hardware tested; raw app, NOT an installer','app_bytes':len(image),'app_limit':0x8dfbc,'data_bss_bytes':used,'data_bss_limit':98304,'pool_bytes':pool,'pool_limit':0x54000,'ram_text_bytes':sym('_rt_end')-sym('_rt_start'),'sha256':hashlib.sha256(image).hexdigest(),'compiler':version.strip()}
    (out/'target-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
except subprocess.CalledProcessError as e:
    print(e.output);raise SystemExit(e.returncode)

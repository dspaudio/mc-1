#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Loader wrapping derived from tools/build.py, Copyright (C) 2026
# Leo Kuroshita (@kurogedelic), Hügelton Instruments.
"""Build app and RAM loader, package pinned SDK assets; never contact hardware."""
import hashlib,json,os,re,struct
from pathlib import Path
import build_target as target
import fm1pkg_make as pkg
import lz4blk
sdk=Path(os.environ['AC79_SDK'])
expected={'uboot.boot':'4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4','cfg_tool.bin':'276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b','cfg/eq_cfg_hw.bin':'41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d'}
for name,digest in expected.items():
    assert hashlib.sha256((sdk/'cpu/wl82/tools'/name).read_bytes()).hexdigest()==digest, name+' SDK hash mismatch'
run=target.run
out=Path('build/loader');out.mkdir(exist_ok=True)
for source,name in [('firmware/loader/crt0_ldr.S','crt0'),('firmware/loader/loader.c','loader')]:
    print(run('pi32v2/bin/clang','-target','pi32v2','-Os','-ffunction-sections','-fno-builtin','-Wall','-Werror','-Wno-unused-function','-Ifirmware/hal','-c',source,'-o',str(out/(name+'.o'))),end='')
print(run('pi32v2/bin/ld','--gc-sections','-e','_start','-T','firmware/loader/loader.ld',str(out/'crt0.o'),str(out/'loader.o'),'-o',str(out/'loader.elf')),end='')
headers=run('common/bin/objdump','-h',str(out/'loader.elf'))
for line in headers.splitlines():
    p=line.split()
    if len(p)>4 and p[1].startswith('.') and int(p[2],16):
        assert 0x1c0a800<=int(p[3],16) and int(p[3],16)+int(p[2],16)<=0x1c1e800, 'Loader outside RAM'
syms=run('common/bin/objdump','-t',str(out/'loader.elf'))
(out/'loader.symbols').write_text(syms)
(out/'loader.dis').write_text(run('common/bin/objdump','-d',str(out/'loader.elf')))
run('common/bin/objcopy','-O','binary','-j','.text',str(out/'loader.elf'),str(out/'loader.bin'))
raw=(out/'loader.bin').read_bytes();assert 0<len(raw)<=0x14000
assert b'FELUCCA-LOADER-1' in raw[:32], 'Upstream updater recognition marker missing'
def head(crc,a,b,attr):
    body=struct.pack('<HIIBBH16s',crc,a,b,attr,0,0,b'usb_hid_ota.bin')
    return struct.pack('<H',pkg.crc16(body))+body
blocks=bytearray()
for pos in range(0,len(raw),4096):
    block=raw[pos:pos+4096];compressed=lz4blk.compress(block)
    assert len(compressed)<=4096 and lz4blk.decompress(compressed)==block
    blocks+=struct.pack('<II',len(compressed),len(block))+compressed
body=head(pkg.crc16(raw),len(raw),0x1c0a800,0)+blocks
ota=head(pkg.crc16(body),32,len(body),0x41)+body
(out/'ota.bin').write_bytes(ota)
pkg.SDK=sdk
app=Path('build/mc-1.app.bin').read_bytes()
# Retain upstream FM-1_900 identity for the existing M-UPGRADE protocol.
image=pkg.ufw(pkg.flash_image(app,pkg.KEY),ota,'FM-1_900')
Path('build/mc-1-EXPERIMENTAL.fwsc').write_bytes(image)
# A different valid old app forces every changed sector through the loader test.
old=bytes([app[0]^1])+app[1:]
Path('build/previous-test-only.fwsc').write_bytes(pkg.ufw(pkg.flash_image(old,pkg.KEY),ota,'FM-1_900'))
report={'status':'Package format and host loader simulation only; NOT device installed or tested','package_bytes':len(image),'sha256':hashlib.sha256(image).hexdigest(),'app_sha256':hashlib.sha256(app).hexdigest(),'loader_bytes':len(raw),'loader_load':'0x01c0a800','loader_bss_end':re.search(r'^([0-9a-f]+).*\s_bss_end$',syms,re.M)[1],'sdk_commit':'d179b4484759423312073f5fbb232501aa491047','sdk_sha256':expected,'identity':'FM-1_900','app_write_range':'0x4000..0x92fff','record_cleanup_range':'0x93000..0xfbfff (only valid update-record sectors)','app_persistence_range':'0x99000..0xaafff'}
Path('build/package-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

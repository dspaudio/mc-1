#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Local interactive panel using the actual C firmware core; no third-party Python modules."""
import argparse, ctypes as C, json, os, struct, subprocess, sys, threading, time, zlib
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=8091);args=parser.parse_args()
lib=ROOT/'build/libmc1.so';lib.parent.mkdir(exist_ok=True)
sources=['src/effects.c','src/demo.c','src/engine.c','src/midi.c','src/factory.c','src/project.c','src/store.c','src/ui.c','host/bridge.c']
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-Isrc',*sources,'-o',str(lib)],cwd=ROOT,check=True)
dsp=C.CDLL(str(lib));dsp.host_init()
dsp.host_audio.restype=C.POINTER(C.c_int16);dsp.host_screen.restype=C.POINTER(C.c_uint16)
dsp.host_tick.argtypes=[C.c_uint32]
dsp.host_action.argtypes=[C.c_uint,C.c_int,C.c_int];dsp.host_import.argtypes=[C.c_void_p,C.c_uint]
dsp.host_project_bytes.restype=C.c_uint
project_bytes: int=dsp.host_project_bytes()
lock=threading.Lock()
def png():
    pixels=dsp.host_screen();raw=bytearray()
    for y in range(240):
        raw.append(0)
        for x in range(240):
            p=pixels[y*240+x];raw.extend(((p>>11)*255//31,((p>>5)&63)*255//63,(p&31)*255//31))
    def chunk(kind,data): return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',240,240,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def send(self,data,typ='text/plain',status=200):
        self.send_response(status);self.send_header('Content-Type',typ);self.send_header('Content-Length',str(len(data)))
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.end_headers();self.wfile.write(data)
    def do_GET(self):
        with lock:
            dsp.host_tick((time.monotonic_ns()//1000000)&0xffffffff)
            if self.path=='/': return self.send((ROOT/'host/index.html').read_bytes(),'text/html; charset=utf-8')
            if self.path=='/screen.png': return self.send(png(),'image/png')
            if self.path=='/audio': return self.send(C.string_at(dsp.host_audio(),4410*4),'application/octet-stream')
            if self.path=='/project':
                blob=C.create_string_buffer(project_bytes);n=dsp.host_export(blob)
                return self.send(blob.raw[:n],'application/octet-stream')
        self.send(b'Not found',status=404)
    def do_POST(self):
        # Reject cross-origin browser writes; bind loopback only. No filesystem paths accepted.
        origin=self.headers.get('Origin')
        if origin and origin not in (f'http://127.0.0.1:{args.port}',f'http://localhost:{args.port}'):
            return self.send(b'Origin rejected',status=403)
        try: size=int(self.headers.get('Content-Length','0'))
        except ValueError: return self.send(b'Bad length',status=400)
        if size<0 or size>(project_bytes if self.path=='/project' else 4096): return self.send(b'Too large',status=413)
        data=self.rfile.read(size)
        with lock:
            dsp.host_tick((time.monotonic_ns()//1000000)&0xffffffff)
            if self.path=='/project':
                if dsp.host_import(data,len(data)): return self.send(b'Invalid project',status=400)
            elif self.path=='/action':
                try:
                    d=json.loads(data);kind=d['kind'];a=d.get('a',0);b=d.get('b',0)
                    if not all(type(x) is int for x in (kind,a,b)) or not(0<=kind<=5 and -127<=a<=127 and -127<=b<=127): raise ValueError()
                    dsp.host_action(kind,a,b)
                except (ValueError,KeyError,TypeError): return self.send(b'Invalid action',status=400)
            else: return self.send(b'Not found',status=404)
        self.send(b'OK')
print(f'MC-1 native host panel: http://127.0.0.1:{args.port} (no hardware access)',flush=True)
try: ThreadingHTTPServer(('127.0.0.1',args.port),Handler).serve_forever()
except KeyboardInterrupt: pass

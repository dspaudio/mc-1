// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Copyright (C) 2026 dspaudio (JavaScript port tests)
// Original deterministic fake-device tests for the product-site installer, 2026-10-15.
import assert from 'node:assert/strict';
import {
    FirmwareInstaller, InstallError, CURRENT_IDENTITY, HS_QUERY, UPGRADE,
    FINISH_CHECK, FINISH_WRITE, pack7, unpack7, parseRequest, parseIdentity,
    response, crc16, jlEnc, packageDamage, loadPackage,
} from '../site/firmware-ota.js';

const bytes = hex => Uint8Array.from(Buffer.from(hex, 'hex'));
const text = value => new TextEncoder().encode(value);
const hex = value => Buffer.from(value).toString('hex');
const u16 = (b, i) => b[i] + b[i + 1] * 256;
const u32 = (b, i) => (b[i] + b[i + 1] * 256 + b[i + 2] * 65536 + b[i + 3] * 16777216) >>> 0;
const set16 = (b, i, n) => new DataView(b.buffer, b.byteOffset, b.byteLength).setUint16(i, n, true);
const set32 = (b, i, n) => new DataView(b.buffer, b.byteOffset, b.byteLength).setUint32(i, n, true);

// Independent bit-position wire oracle; do not round-trip the host encoder into itself.
function encode(data) {
    const out = new Uint8Array(Math.ceil(data.length * 8 / 7));
    for (let bit = 0; bit < data.length * 8; bit++)
        out[Math.floor(bit / 7)] |= ((data[Math.floor(bit / 8)] >>> (bit % 8)) & 1) << (bit % 7);
    return Uint8Array.of(0xf0, ...out, 0xf7);
}
function decode(packet) {
    assert.equal(packet[0], 0xf0);
    assert.equal(packet.at(-1), 0xf7);
    const packed = packet.slice(1, -1);
    assert(packed.every(b => b < 128));
    const out = new Uint8Array(Math.floor(packed.length * 7 / 8));
    for (let bit = 0; bit < out.length * 8; bit++)
        out[Math.floor(bit / 8)] |= ((packed[Math.floor(bit / 7)] >>> (bit % 7)) & 1) << (bit % 8);
    assert.equal(hex(encode(out)), hex(packet));
    return out;
}
function frame(cmd, data) {
    const b = new Uint8Array(7 + data.length);
    b.set([0, 0x59, cmd]);
    set32(b, 3, data.length);
    b.set(data, 6);
    b[b.length - 1] = 255 - (data.reduce((s, n) => s + n, 0) & 255);
    return encode(b);
}
function identity(value) {
    const body = new Uint8Array(27);
    body.set(text(value));
    return frame(0x11, body);
}
function request(address, length, flags = 0x85) {
    const body = new Uint8Array(8);
    body[0] = flags;
    set32(body, 1, address);
    body[5] = length & 255;
    body[6] = (length >>> 8) & 255;
    body[7] = length >>> 16;
    return frame(0x30, body);
}

// Independent table-based CRC and stream cipher for package/fake-device validation.
const table = Array.from({length: 256}, (_, i) => {
    let n = i * 256;
    for (let bit = 0; bit < 8; bit++) n = (n & 32768) ? (n * 2) ^ 0x1021 : n * 2;
    return n & 65535;
});
function crc(data) {
    let n = 0;
    for (const b of data) n = ((n << 8) ^ table[(n >>> 8) ^ b]) & 65535;
    return n;
}
function cipher(data) {
    let k = 65535;
    return Uint8Array.from(data, b => {
        const out = b ^ (k % 256);
        k = ((k * 2) ^ (k >= 32768 ? 0x1021 : 0)) & 65535;
        return out;
    });
}
function rawPackage(image, product = CURRENT_IDENTITY) {
    const raw = new Uint8Array(image.length + 20);
    for (let i = 0; i < 20; i++) {
        raw.set(image.slice(i * 47, (i + 1) * 47), i * 48);
        raw[i * 48 + 47] = i < product.length ? (product.charCodeAt(i) + i + 1) & 255 : 0x7d;
    }
    raw.set(image.slice(940), 960);
    return raw;
}
function fixture() {
    const flashOffset = 1024, flashLength = 0x93000;
    const otaOffset = flashOffset + flashLength, otaLength = 600;
    const image = new Uint8Array(otaOffset + otaLength);
    for (let i = flashOffset; i < image.length; i++) image[i] = (i * 17 + (i >>> 8)) & 255;
    image.set(text('FELUCCA-LOADER-1'), otaOffset + 0x48);
    const list = new Uint8Array(160);
    for (const [i, type, name, off, len] of [
        [0, 0, 'flash.bin', flashOffset, flashLength], [1, 100, 'ota.bin', otaOffset, otaLength],
    ]) {
        const e = new Uint8Array(80);
        set16(e, 0, type);
        set16(e, 2, i);
        set16(e, 4, crc(image.slice(off, off + len)));
        set32(e, 8, off);
        set32(e, 12, len);
        e.set(text(name), 64);
        list.set(cipher(e), i * 80);
    }
    const header = new Uint8Array(64);
    set16(header, 2, crc(list));
    set32(header, 4, image.length);
    set16(header, 8, 2);
    set16(header, 0, crc(header.slice(2)));
    image.set(cipher(header));
    image.set(list, 64);
    return {image, raw: rawPackage(image), flashOffset, flashLength, otaOffset, otaLength};
}
const pkg = fixture();

// Fixed vectors produced directly by the Python installer, not this module.
assert.equal(hex(pack7(bytes('007f80ff55aa01fe'))), '007e017c5f4a6a007e01');
assert.equal(hex(unpack7(bytes('007e017c5f4a6a007e01'))), '007f80ff55aa01fe');
assert.equal(hex(response(0x12345678, bytes('007f80ff'), 5)), 'f00032416100004002782c511141000000007e017c4f1cf7');
assert.equal(hex(jlEnc(Uint8Array.from({length: 16}, (_, i) => i))), 'ffde9d1c1b3b7afff8c8a96cc2b05598');
assert.equal(crc16(text('123456789')), 0x31c3);
assert.equal(crc(text('123456789')), 0x31c3);
assert.equal(parseIdentity(identity('FM-1_900')).text, 'FM-1_900');
assert.equal(parseIdentity(identity('FM-1_015')).loader, false);
assert.equal(parseIdentity(identity('ota-FM-1_900')).loader, true);
assert.equal(parseIdentity(identity('MC-1')), null);
assert.deepEqual(parseRequest(request(0x12345678, 512)), {flags: 0x85, address: 0x12345678, length: 512});
assert.equal(parseRequest(request(0, 0xffffff)).length, 0xffffff);
const corrupt = request(0, 512);
corrupt[8] ^= 1;
assert.equal(parseRequest(corrupt), null);
const malformed = decode(request(0, 512));
malformed[3] = 9;
assert.equal(parseRequest(encode(malformed)), null);
const badIdentity = identity('FM-1_900');
badIdentity[10] ^= 1;
assert.equal(parseIdentity(badIdentity), null);
assert.equal(parseIdentity(Uint8Array.of(...identity('FM-1_900').slice(0, -1), 0, 0xf7)), null);
assert.equal(response(0, new Uint8Array(512)).length, 605);
assert.throws(() => response(0, new Uint8Array(513)), {code: 'badreq'});
for (let n = 0; n <= 512; n++) {
    const data = Uint8Array.from({length: n}, (_, i) => (i * 131 + n) & 255);
    assert.deepEqual(unpack7(pack7(data)), data);
    assert.equal(hex(pack7(data)), hex(encode(data).slice(1, -1)));
}
assert.equal(packageDamage(pkg.image), null);
assert.equal((await loadPackage(pkg.raw)).product, 'FM-1_900');
assert.deepEqual((await loadPackage(pkg.raw)).image, pkg.image);
const digest = hex(new Uint8Array(await crypto.subtle.digest('SHA-256', pkg.raw)));
await loadPackage(pkg.raw.buffer, {sha256: digest.toUpperCase()});
const mutable = pkg.raw.slice();
const snapshot = loadPackage(mutable, {sha256: digest});
mutable.fill(0);
assert.deepEqual((await snapshot).image, pkg.image, 'Package cannot change during digest validation');
await assert.rejects(loadPackage(pkg.raw, {sha256: '0'.repeat(64)}), {code: 'badpkg'});
await assert.rejects(loadPackage(pkg.raw, {sha256: 'bad'}), {code: 'badpkg'});
await assert.rejects(loadPackage(new Uint8Array(959)), {code: 'badpkg'});
await assert.rejects(loadPackage(rawPackage(pkg.image, 'MC-1_900')), {code: 'badpkg'});
const noMarker = pkg.raw.slice();
const markerIndex = Buffer.from(noMarker).indexOf('FELUCCA-LOADER-1');
noMarker[markerIndex] = 0;
await assert.rejects(loadPackage(noMarker), {code: 'badpkg'});
for (const [offset, expected] of [[10, 'header CRC'], [70, 'file list CRC'], [1024, 'flash.bin: CRC']]) {
    const damaged = pkg.image.slice();
    damaged[offset] ^= 1;
    assert.equal(packageDamage(damaged), expected);
    await assert.rejects(loadPackage(rawPackage(damaged)), {code: 'badpkg'});
}
function editHeader(change) {
    const image = pkg.image.slice(), h = cipher(image.slice(0, 64));
    change(h);
    set16(h, 0, crc(h.slice(2)));
    image.set(cipher(h));
    return image;
}
assert.equal(packageDamage(editHeader(h => set16(h, 8, 9))), 'file list');
const cut = pkg.image.slice();
const entry = cipher(cut.slice(64, 144));
set32(entry, 12, cut.length);
cut.set(cipher(entry), 64);
const h = cipher(cut.slice(0, 64));
set16(h, 2, crc(cut.slice(64, 224)));
set16(h, 0, crc(h.slice(2)));
cut.set(cipher(h));
assert.equal(packageDamage(cut), 'flash.bin: cut short');

class Clock {
    constructor() {
        this.time = 0; this.timers = new Map(); this.observers = []; this.clearObservers = []; this.serial = 0;
    }
    now = () => this.time;
    setTimeout = (fn, ms) => {
        const id = ++this.serial;
        this.timers.set(id, {fn, ms, at: this.time + ms});
        for (const o of this.observers.slice()) if (o.ms === ms) {
            this.observers.splice(this.observers.indexOf(o), 1);
            o.resolve(id);
        }
        return id;
    };
    clearTimeout = id => {
        const timer = this.timers.get(id);
        this.timers.delete(id);
        if (timer) for (const o of this.clearObservers.slice()) if (o.ms === timer.ms) {
            this.clearObservers.splice(this.clearObservers.indexOf(o), 1);
            o.resolve();
        }
    };
    scheduled(ms) {
        return new Promise(resolve => this.observers.push({ms, resolve}));
    }
    cleared(ms) {
        return new Promise(resolve => this.clearObservers.push({ms, resolve}));
    }
    fire(id) {
        const timer = this.timers.get(id);
        assert(timer, 'Only a currently subscribed timer may fire');
        this.timers.delete(id);
        this.time = timer.at;
        timer.fn();
    }
}

class Events {
    constructor() { this.handlers = new Map(); }
    addEventListener(type, fn) {
        if (!this.handlers.has(type)) this.handlers.set(type, new Set());
        this.handlers.get(type).add(fn);
    }
    removeEventListener(type, fn) { this.handlers.get(type)?.delete(fn); }
    emit(type, event) { for (const fn of [...(this.handlers.get(type) || [])]) fn(event); }
    count() { return [...this.handlers.values()].reduce((n, set) => n + set.size, 0); }
}
class FakePort extends Events {
    constructor(device, type, stage, name) {
        super();
        Object.assign(this, {device, type, stage, name, id: `${stage}-${type}`, state: 'connected', connection: 'closed'});
    }
    async open() {
        this.connection = 'open';
        this.device.access.emit('statechange', {port: this});
        return this;
    }
    async close() {
        this.connection = 'closed';
        this.device.access.emit('statechange', {port: this});
        return this;
    }
    send(packet) {
        assert.equal(this.connection, 'open', 'Never send through a closed output');
        assert.equal(this.state, 'connected', 'Never send through a disconnected output');
        this.device.receive(packet, this.stage);
    }
}
class FakeDevice {
    constructor(options = {}) {
        this.options = options;
        this.access = Object.assign(new Events(), {inputs: new Map(), outputs: new Map(), sysexEnabled: true});
        this.ports = [];
        this.upgrades = [];
        this.responses = 0;
        this.receivedFlash = new Uint8Array(pkg.flashLength);
        this.connect('initial', options.initial || 'FM-1_015');
    }
    connect(stage, product) {
        this.product = product;
        this.stage = stage;
        const name = stage === 'loader' ? 'Felucca Update' : 'FM-1';
        this.input = new FakePort(this, 'input', stage, name);
        this.output = new FakePort(this, 'output', stage, name);
        this.ports.push(this.input, this.output);
        this.access.inputs.set(this.input.id, this.input);
        this.access.outputs.set(this.output.id, this.output);
        // Realistic separate input/output arrival events, not one atomic magic reconnect.
        this.access.emit('statechange', {port: this.input});
        this.access.emit('statechange', {port: this.output});
    }
    disconnect() {
        for (const port of [this.input, this.output]) {
            port.state = 'disconnected';
            this.access.emit('statechange', {port});
        }
    }
    emit(packet) { this.input.emit('midimessage', {data: packet}); }
    next() {
        const item = this.plan.shift();
        assert(item);
        this.expected = item;
        this.emit(request(...item));
    }
    receive(packet, stage) {
        assert.equal(stage, this.stage);
        if (this.options.failSend?.(packet, stage)) throw new Error('Fake output failure');
        if (hex(packet) === hex(HS_QUERY)) {
            if (!this.options.noIdentity) this.emit(identity(this.product));
            return;
        }
        if (hex(packet) === hex(UPGRADE)) {
            this.upgrades.push(this.stage);
            if (this.options.onUpgrade) this.options.onUpgrade(this);
            if (this.options.unplug === this.stage) { this.disconnect(); return; }
            if (this.options.stall === this.stage) return;
            if (this.options.badRequest) {
                this.plan = [this.options.badRequest];
            } else if (this.product.startsWith('ota-')) {
                this.plan = [[0, 512], [512, 512], [pkg.flashOffset + 0x4000, 32]];
                for (let off = 0; off < pkg.flashLength; off += 512)
                    this.plan.push([pkg.flashOffset + off, Math.min(512, pkg.flashLength - off)]);
                // The real loader may reread any block (CRC retry/equal-sector passes).
                this.plan.push([pkg.flashOffset, 512], [FINISH_WRITE, 8]);
            } else {
                this.plan = [[0, 512], [512, 512], [pkg.otaOffset, 512], [pkg.otaOffset, 512],
                    [pkg.otaOffset + 32, 512], [pkg.otaOffset + 544, 56], [FINISH_CHECK, 8]];
            }
            if (this.options.noise) {
                this.emit(Uint8Array.of(0xf0, 1, 2, 0xf7));
                const invalid = request(0, 512); invalid[5] ^= 1; this.emit(invalid);
            }
            this.next();
            return;
        }
        const b = decode(packet);
        assert.equal(b[2], 0x30);
        assert.equal(u32(b, 3) & 0xffffff, b.length - 7);
        assert.equal(b.slice(6).reduce((s, n) => s + n, 0) & 255, 255, 'Device validates response checksum');
        const [address, length] = this.expected;
        assert.equal(u32(b, 7), address);
        assert.equal(u32(b, 11) & 0xffffff, length);
        assert.equal(b.length, length + 15);
        const data = b.slice(14, -1);
        this.responses++;
        if (address === FINISH_CHECK || address === FINISH_WRITE) {
            assert.equal(b[6], 0);
            assert.deepEqual(data, text('success\0'));
            this.disconnect();
            if (address === FINISH_CHECK) {
                if (!this.options.noLoader) this.connect('loader', this.options.loader || 'ota-FM-1_900');
            } else {
                assert.equal(crc(this.receivedFlash), u16(cipher(pkg.image.slice(64, 144)), 4),
                    'Device only reboots after receiving the entire flash file with matching CRC');
                if (!this.options.noReboot) this.connect('reboot', this.options.returned || CURRENT_IDENTITY);
            }
            return;
        }
        assert.equal(b[6], 0x85, 'Device requires echoed request flags');
        assert.deepEqual(data, pkg.image.slice(address, address + length));
        if (this.product.startsWith('ota-') && address >= pkg.flashOffset && address + length <= pkg.otaOffset)
            this.receivedFlash.set(data, address - pkg.flashOffset);
        this.next();
    }
    installer(extra = {}) {
        this.clock = new Clock();
        this.progress = [];
        this.inst = new FirmwareInstaller({access: this.access, input: this.input, output: this.output,
            clock: this.clock, ...extra, onProgress: event => {
                this.progress.push(event);
                extra.onProgress?.(event);
            }});
        return this.inst;
    }
    clean() {
        assert.equal(this.access.count(), 0, 'Access listeners removed');
        assert(this.ports.every(p => p.count() === 0), 'Input listeners removed');
        assert(this.ports.every(p => p.connection === 'closed'), 'All owned ports closed');
        assert.equal(this.clock.timers.size, 0, 'No timer leaks');
        assert.equal(this.inst.running, false);
    }
}

let scenarios = 0;
async function success(options = {}, extra = {}, installOptions = {}) {
    const device = new FakeDevice(options);
    const result = await device.installer(extra).install(pkg.raw, installOptions);
    assert.equal(result.identity, 'FM-1_900');
    assert.equal(device.progress.at(-1).stage, 'done');
    assert(device.progress.filter(e => ['check', 'write', 'reboot'].includes(e.stage)).every(e => !e.canCancel));
    assert(device.responses > 1000);
    device.clean();
    scenarios++;
    return device;
}
const normal = await success({noise: true});
assert.deepEqual(normal.upgrades, ['initial', 'loader']);
assert.equal(normal.progress.filter(e => e.stage === 'check').at(-1).requests, 6);
const resumed = await success({initial: 'ota-FM-1_900'});
assert.deepEqual(resumed.upgrades, ['initial']);
await success({initial: 'FM-1_900'}, {onProgress() { throw new Error('Broken UI'); }});
await success({initial: 'FM-1_999'});
const unsafeAbort = new AbortController();
await success({onUpgrade() { unsafeAbort.abort(); }}, {}, {signal: unsafeAbort.signal});

async function failure(options, code, expectedUpgrades, extra = {}, installOptions = {}, raw = pkg.raw) {
    const device = new FakeDevice(options);
    const installer = device.installer(extra);
    await assert.rejects(installer.install(raw, installOptions), error => {
        assert(error instanceof InstallError);
        assert.equal(error.code, code);
        assert.equal(error.recoveryRequired, expectedUpgrades > 0);
        return true;
    });
    assert.equal(device.upgrades.length, expectedUpgrades);
    device.clean();
    scenarios++;
    return device;
}
await failure({initial: 'FM-2_900'}, 'model', 0);
await failure({initial: 'ota-FM-1_015'}, 'foreign', 0);
await failure({loader: 'ota-FM-1_015'}, 'foreign', 1);
await failure({badRequest: [0, 513]}, 'badreq', 1);
await failure({badRequest: [pkg.image.length, 1]}, 'badreq', 1);
await failure({badRequest: [0xffffffff, 1]}, 'badreq', 1);
await failure({unplug: 'initial'}, 'lost', 1);
await failure({unplug: 'loader'}, 'lost', 2);
await failure({returned: 'FM-1_901'}, 'mismatch', 2);
await failure({}, 'permission', 0, {access: {sysexEnabled: false, removeEventListener() {}}});
const aborted = new AbortController(); aborted.abort();
await failure({}, 'cancelled', 0, {}, {signal: aborted.signal});
const readyAbort = new AbortController();
await failure({}, 'cancelled', 0, {onProgress(e) { if (e.stage === 'ready') readyAbort.abort(); }}, {signal: readyAbort.signal});
await failure({}, 'badpkg', 0, {}, {sha256: '0'.repeat(64)});
await failure({}, 'badpkg', 0, {}, {}, noMarker);
await failure({failSend: p => hex(p) === hex(HS_QUERY)}, 'lost', 0);
{
    // Upgrade submission itself failed, but the installer must conservatively report unsafe:
    // a transport exception cannot prove the hardware did not receive the command.
    const device = new FakeDevice({failSend: p => hex(p) === hex(UPGRADE)});
    await assert.rejects(device.installer().install(pkg.raw), {code: 'lost', recoveryRequired: true});
    assert.equal(device.upgrades.length, 0);
    device.clean();
    scenarios++;
}
await failure({failSend: p => {
    if (hex(p) === hex(HS_QUERY) || hex(p) === hex(UPGRADE)) return false;
    return u32(decode(p), 7) === FINISH_CHECK;
}}, 'lost', 1);

{
    // Connection events (not a retry timer) wake the installer when the loader arrives later.
    const device = new FakeDevice({noLoader: true}), installer = device.installer();
    const waiting = device.clock.scheduled(30000);
    const result = installer.install(pkg.raw);
    await waiting;
    const unrelated = new FakePort(device, 'output', 'unrelated', 'Unrelated Synth');
    unrelated.send = () => { throw new Error('Unrelated gear must not be probed'); };
    device.access.outputs.set(unrelated.id, unrelated);
    device.access.emit('statechange', {port: unrelated});
    device.connect('loader', 'ota-FM-1_900');
    assert.equal((await result).identity, CURRENT_IDENTITY);
    device.clean();
    scenarios++;
}
{
    // A newly enumerated second FM-1 must not silently become the flash target.
    const device = new FakeDevice({noLoader: true}), installer = device.installer();
    const waiting = device.clock.scheduled(30000);
    const result = installer.install(pkg.raw);
    const rejection = assert.rejects(result, {code: 'ambiguous', recoveryRequired: true});
    await waiting;
    device.connect('loader', 'ota-FM-1_900');
    const otherInput = new FakePort(device, 'input', 'other', 'FM-1 Other');
    const otherOutput = new FakePort(device, 'output', 'other', 'FM-1 Other');
    device.access.inputs.set(otherInput.id, otherInput);
    device.access.outputs.set(otherOutput.id, otherOutput);
    device.access.emit('statechange', {port: otherInput});
    await rejection;
    device.clean();
    scenarios++;
}
{
    const abort = new AbortController();
    const device = new FakeDevice({noIdentity: true}), installer = device.installer();
    const waiting = device.clock.scheduled(1000);
    const result = installer.install(pkg.raw, {signal: abort.signal});
    const rejection = assert.rejects(result, {code: 'cancelled', recoveryRequired: false});
    await waiting;
    abort.abort();
    await rejection;
    assert.equal(device.upgrades.length, 0);
    device.clean();
    scenarios++;
}
{
    // A stale open completion is closed even after the failed run has returned.
    const device = new FakeDevice(), installer = device.installer();
    const originalOpen = device.input.open.bind(device.input);
    let releaseOpen, closedLate;
    const lateClosed = new Promise(resolve => { closedLate = resolve; });
    const originalClose = device.input.close.bind(device.input);
    device.input.open = () => new Promise(resolve => { releaseOpen = async () => resolve(await originalOpen()); });
    let closeCount = 0;
    device.input.close = async () => {
        await originalClose();
        if (++closeCount === 2) closedLate();
    };
    const waiting = device.clock.scheduled(3000);
    const result = installer.install(pkg.raw);
    const rejection = assert.rejects(result, {code: 'timeout', recoveryRequired: false});
    device.clock.fire(await waiting);
    await rejection;
    const lateSettled = device.clock.cleared(3000);
    await releaseOpen();
    await lateClosed;
    await lateSettled;
    assert.equal(device.upgrades.length, 0);
    device.clean();
    scenarios++;
}
{
    // Valid requests cannot keep an installation alive beyond the absolute deadline.
    const device = new FakeDevice();
    const installer = device.installer({onProgress(e) {
        if (e.stage === 'check' && e.requests) device.clock.time += 60000;
    }});
    await assert.rejects(installer.install(pkg.raw), {code: 'timeout', recoveryRequired: true});
    assert.equal(device.upgrades.length, 1);
    device.clean();
    scenarios++;
}

// Subscribe to the exact timeout registration before triggering installation.
// Advance virtual time once; no wall-clock sleeps, polling, or timing-luck assertions.
for (const [options, duration, code] of [
    [{stall: 'initial'}, 8000, 'timeout'],
    [{stall: 'loader'}, 180000, 'timeout'],
    [{noLoader: true}, 30000, 'noloader'],
    [{noReboot: true}, 40000, 'noreturn'],
]) {
    const device = new FakeDevice(options), installer = device.installer();
    const subscribed = device.clock.scheduled(duration);
    const result = installer.install(pkg.raw);
    const rejection = assert.rejects(result, {code, recoveryRequired: true});
    device.clock.fire(await subscribed);
    await rejection;
    device.clean();
    scenarios++;
}
{
    const device = new FakeDevice({noIdentity: true}), installer = device.installer();
    let subscribed = device.clock.scheduled(1000);
    const result = installer.install(pkg.raw);
    const rejection = assert.rejects(result, {code: 'timeout', recoveryRequired: false});
    for (let attempt = 0; attempt < 3; attempt++) {
        const id = await subscribed;
        subscribed = attempt < 2 ? device.clock.scheduled(1000) : null;
        device.clock.fire(id);
    }
    await rejection;
    device.clean();
    scenarios++;
}
console.log(JSON.stringify({firmwareOta: 'PASS', scenarios,
    vectors: ['Python pack7/response/ENC/CRC', '0..512-byte packing', 'identity/checksum/framing',
        'package markers/model/header/list/files/CRC/SHA256'],
    hardwareAccess: false}));

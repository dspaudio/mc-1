// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Copyright (C) 2026 dspaudio (JavaScript port and integration)
// JavaScript port and Web MIDI integration, modified 2026-10-15.
// Derived from dspaudio/orbit tools/fm1_install.py; provenance is in NOTICE.md.
//
// UI contract:
//   new FirmwareInstaller({access, input, output, onProgress}).install(raw, {sha256, signal})
// access must have sysexEnabled, obtained by the UI with requestMIDIAccess({sysex:true}).
// input/output are uniquely discovered MIDI ports, exclusively owned during install.
// raw is a Uint8Array or ArrayBuffer; sha256 is an optional trusted 64-digit hex digest
// of the ORIGINAL .fwsc bytes (not the logical image). No fetching or permission UI here.
// Progress: {stage, canCancel, requests?, end?, total?, identity?}. Cancellation is
// accepted only before the FIRST upgrade command: even "check" stages a flash loader.
// After that, abort is ignored and service continues through verified reboot.
// Reconnection uses MIDI statechange events, never polling. Only recognizable, uniquely
// paired MIDI ports are probed after re-enumeration; no first-output fallback.
// The UI must stop normal MIDI/audio traffic, keep this page alive during installation,
// show recoveryRequired errors, and arrange a backup before offering an update.
// Firmware writes [0x4000,0x93000), stages [0xE0000,0xE5000), and erases only
// valid update-record sectors in [0x93000,0xFC000). MC-1 project banks at
// [0x99000,0xAB000) and globals/tail at [0xFC000,0x100000) are not reformatted.
// This is not a backup/migration protocol and cannot prove saved data retention.
// FM-1_900 is the protocol identity, NOT a unique MC-1 build/version identifier.

export const CURRENT_IDENTITY = 'FM-1_900';
export const HS_QUERY = Uint8Array.of(0xf0, 0, 0x32, 0x45, 0, 0, 0, 0x40, 0x7f, 0xf7);
export const UPGRADE = Uint8Array.of(0xf0, 0x22, 0x24, 0x35, 0x7f, 0xf7);
export const FINISH_CHECK = 0xe0000000;
export const FINISH_WRITE = 0xf0000000;
export const MAX_DATA = 512;
const encoder = new TextEncoder();
const latin1 = bytes => String.fromCharCode(...bytes);
const rd16 = (b, i) => b[i] | b[i + 1] << 8;
const rd24 = (b, i) => rd16(b, i) | b[i + 2] << 16;
const rd32 = (b, i) => (rd24(b, i) | b[i + 3] << 24) >>> 0;
const wr32 = (b, i, n) => { for (let k = 0; k < 4; k++) b[i + k] = n >>> (k * 8); };
const checksum = b => ~b.reduce((s, n) => s + n, 0) & 255;

export class InstallError extends Error {
    constructor(code, message, recoveryRequired = false) {
        super(message);
        this.name = 'InstallError';
        this.code = code;
        this.recoveryRequired = recoveryRequired;
    }
}

export function pack7(data) {
    const out = [];
    let acc = 0, bits = 0;
    for (const b of data) {
        acc |= b << bits;
        bits += 8;
        while (bits >= 7) {
            out.push(acc & 127);
            acc >>>= 7;
            bits -= 7;
        }
    }
    if (bits) out.push(acc & 127);
    return Uint8Array.from(out);
}

export function unpack7(data) {
    const out = [];
    let acc = 0, bits = 0;
    for (const b of data) {
        acc |= (b & 127) << bits;
        bits += 7;
        while (bits >= 8) {
            out.push(acc & 255);
            acc >>>= 8;
            bits -= 8;
        }
    }
    return Uint8Array.from(out);
}

function decode(packet) {
    if (packet.length < 2 || packet[0] !== 0xf0 || packet.at(-1) !== 0xf7) return null;
    const wire = packet.slice(1, -1);
    if (wire.some(b => b > 127)) return null;
    const body = unpack7(wire);
    // Reject noncanonical padding and lengths, not merely a plausible decoded prefix.
    if (!pack7(body).every((b, i) => b === wire[i]) || pack7(body).length !== wire.length) return null;
    if (body.length < 7 || body[0] !== 0 || body[1] !== 0x59 ||
        rd24(body, 3) + 7 !== body.length ||
        checksum(body.slice(6, -1)) !== body.at(-1)) return null;
    return body;
}

export function parseRequest(packet) {
    const b = decode(packet);
    if (!b || b.length !== 15 || b[2] !== 0x30) return null;
    return {flags: b[6], address: rd32(b, 7), length: rd24(b, 11)};
}

export function parseIdentity(packet) {
    const b = decode(packet);
    if (!b || b.length !== 34 || b[2] !== 0x11) return null;
    const text = latin1(b.slice(6, 33)).replace(/\0+$/, '');
    const match = /^([^_]+)_(\d+)$/.exec(text);
    return match ? {text, model: match[1], version: Number(match[2]), loader: /^ota-/i.test(match[1])} : null;
}

export function response(address, data, flags = 0) {
    if (data.length > MAX_DATA) throw new InstallError('badreq', 'Response exceeds 512 bytes');
    const b = new Uint8Array(15 + data.length);
    b.set([0, 0x59, 0x30, (data.length + 8) & 255, (data.length + 8) >>> 8, 0, flags]);
    wr32(b, 7, address);
    b[11] = data.length & 255;
    b[12] = data.length >>> 8;
    b.set(data, 14);
    b[b.length - 1] = checksum(b.slice(6, -1));
    return Uint8Array.of(0xf0, ...pack7(b), 0xf7);
}

export function crc16(data, crc = 0) {
    for (const b of data) {
        crc ^= b << 8;
        for (let i = 0; i < 8; i++) crc = ((crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0)) & 0xffff;
    }
    return crc;
}

export function jlEnc(bytes) {
    const out = bytes.slice();
    let key = 0xffff;
    for (let i = 0; i < out.length; i++) {
        out[i] ^= key & 255;
        key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xffff;
    }
    return out;
}

export function packageDamage(image) {
    if (image.length < 0x40) return 'too short';
    const h = jlEnc(image.slice(0, 0x40));
    if (crc16(h.slice(2)) !== rd16(h, 0)) return 'header CRC';
    const count = rd16(h, 8);
    // Deliberately retain the Python installer's stricter limit (firmware allows 11).
    if (count < 1 || count > 8 || image.length < 0x40 + 0x50 * count) return 'file list';
    const list = image.slice(0x40, 0x40 + 0x50 * count);
    if (crc16(list) !== rd16(h, 2)) return 'file list CRC';
    for (let i = 0; i < count; i++) {
        const e = jlEnc(list.slice(i * 0x50, (i + 1) * 0x50));
        const off = rd32(e, 8), size = rd32(e, 12);
        const name = latin1(e.slice(0x40, 0x50)).split('\0')[0];
        if (off + size > image.length) return `${name}: cut short`;
        if (crc16(image.slice(off, off + size)) !== rd16(e, 4)) return `${name}: CRC`;
    }
    return null;
}

export async function loadPackage(bytes, {sha256} = {}) {
    // Copy first: a caller must not mutate validated bytes during an async digest or flash.
    const raw = bytes instanceof ArrayBuffer ? new Uint8Array(bytes.slice(0)) : Uint8Array.from(bytes);
    const bad = message => { throw new InstallError('badpkg', message); };
    if (raw.length < 20 * 48) bad('Not an FM-1 package: too short');
    if (sha256 !== undefined) {
        if (!/^[0-9a-f]{64}$/i.test(sha256)) bad('Invalid SHA256 digest');
        const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', raw));
        const hex = Array.from(digest, b => b.toString(16).padStart(2, '0')).join('');
        if (hex !== sha256.toLowerCase()) bad('Package SHA256 mismatch');
    }
    let product = '';
    const image = new Uint8Array(raw.length - 20);
    for (let i = 0; i < 20; i++) {
        const marker = raw[i * 48 + 47];
        if (marker !== 0x7d) product += String.fromCharCode((marker - i - 1) & 255);
        image.set(raw.slice(i * 48, i * 48 + 47), i * 47);
    }
    image.set(raw.slice(960), 940);
    if (!/^FM-1_\d+$/.test(product)) bad(`Not an FM-1 package: identity ${product}`);
    const marker = encoder.encode('FELUCCA-LOADER-1');
    if (!raw.some((_, i) => marker.every((b, k) => raw[i + k] === b))) bad('No Felucca update loader marker');
    const damage = packageDamage(image);
    if (damage) bad(`Damaged package: ${damage}`);
    return {product, image};
}

const defaultClock = {
    now: () => performance.now(),
    setTimeout: (fn, ms) => setTimeout(fn, ms),
    clearTimeout: id => clearTimeout(id),
};

// One bounded event mailbox; listeners are installed before any open/send action.
class Inbox {
    constructor(clock) {
        this.clock = clock;
        this.queue = [];
        this.pending = null;
        this.error = null;
    }
    push(value) {
        if (this.error) return;
        if (this.pending) this.pending(null, value);
        else if (this.queue.length < 128) this.queue.push(value);
        else this.fail(new InstallError('overflow', 'MIDI event queue overflow'));
    }
    fail(error) {
        this.error = error;
        if (this.pending) this.pending(error);
    }
    next(ms, signal) {
        if (signal?.aborted) return Promise.reject(new InstallError('cancelled', 'Cancelled before upgrade'));
        if (this.error) return Promise.reject(this.error);
        if (this.queue.length) return Promise.resolve(this.queue.shift());
        return new Promise((resolve, reject) => {
            const done = (error, value) => {
                this.clock.clearTimeout(timer);
                signal?.removeEventListener('abort', abort);
                this.pending = null;
                error ? reject(error) : resolve(value);
            };
            const abort = () => done(new InstallError('cancelled', 'Cancelled before upgrade'));
            const timer = this.clock.setTimeout(() => done(new InstallError('timeout', 'Device response timed out')), Math.max(0, ms));
            this.pending = done;
            signal?.addEventListener('abort', abort, {once: true});
        });
    }
}

class Link {
    constructor(owner, input, output) {
        this.owner = owner;
        this.input = input;
        this.output = output;
        this.inbox = new Inbox(owner.clock);
        this.message = event => { if (event.data[0] === 0xf0) this.inbox.push(Uint8Array.from(event.data)); };
        this.state = () => {
            if (input.state === 'disconnected' || output.state === 'disconnected')
                this.inbox.fail(new InstallError('lost', 'Selected MIDI device disconnected'));
        };
        input.addEventListener('midimessage', this.message);
        owner.access.addEventListener('statechange', this.state);
        this.state();
    }
    send(bytes) {
        this.state();
        if (this.inbox.error) throw this.inbox.error;
        try { this.output.send(bytes); }
        catch (error) { throw new InstallError('lost', `MIDI send failed: ${error.message}`); }
    }
    async close() {
        this.closed = true;
        this.input.removeEventListener('midimessage', this.message);
        this.owner.access.removeEventListener('statechange', this.state);
        this.inbox.fail(new InstallError('closed', 'MIDI link closed'));
        await Promise.all([this.input, this.output].map(async port => {
            try { await this.owner.bounded(port.close(), this.owner.timeouts.open); }
            catch (error) { this.owner.cleanupErrors.push(error); }
        }));
    }
}

export class FirmwareInstaller {
    constructor({access, input, output, onProgress = () => {}, clock = defaultClock, timeouts = {}}) {
        this.access = access;
        this.input = input;
        this.output = output;
        this.onProgress = onProgress;
        this.clock = clock;
        this.timeouts = {open: 3000, handshake: 1000, check: 8000, write: 180000,
            loader: 30000, reboot: 40000, checkTotal: 120000, writeTotal: 900000, ...timeouts};
        for (const n of Object.values(this.timeouts))
            if (!Number.isFinite(n) || n <= 0) throw new TypeError('Timeouts must be finite positive milliseconds');
        this.running = false;
        this.unsafe = false;
        this.cleanupErrors = [];
        this.progressErrors = [];
    }
    progress(stage, detail = {}) {
        // A rendering error must never abandon a flash transaction.
        try { this.onProgress({stage, canCancel: !this.unsafe, ...detail}); }
        catch (error) { this.progressErrors.push(error); }
    }
    async bounded(promise, ms, signal) {
        const inbox = new Inbox(this.clock);
        Promise.resolve(promise).then(value => inbox.push(value), error => inbox.fail(error));
        return inbox.next(ms, signal);
    }
    async open(input, output, signal, ms = this.timeouts.open) {
        const link = new Link(this, input, output);
        try {
            const opening = Promise.all([input, output].map(async port => {
                await port.open();
                // Web MIDI open() is not abortable. Reclose a late completion after timeout.
                if (link.closed) await this.bounded(port.close(), this.timeouts.open);
            }));
            await this.bounded(opening, ms, signal);
            link.state();
            if (link.inbox.error) throw link.inbox.error;
            return link;
        } catch (error) {
            await link.close();
            throw error;
        }
    }
    async handshake(link, signal, deadline = Infinity) {
        for (let attempt = 0; attempt < 3; attempt++) {
            const end = Math.min(deadline, this.clock.now() + this.timeouts.handshake);
            if (end <= this.clock.now()) throw new InstallError('timeout', 'Identity response timed out');
            // Arm response subscription before send, including synchronous test devices.
            let pending = link.inbox.next(end - this.clock.now(), signal);
            try {
                link.send(HS_QUERY);
                while (true) {
                    const identity = parseIdentity(await pending);
                    if (identity) return identity;
                    if (this.clock.now() >= end) throw new InstallError('timeout', 'Identity response timed out');
                    pending = link.inbox.next(end - this.clock.now(), signal);
                }
            } catch (error) {
                // A failed send must also consume the already-armed wait.
                link.inbox.fail(error);
                await pending.catch(() => {});
                if (error.code !== 'timeout' || attempt === 2) throw error;
                link.inbox.error = null;
            }
        }
    }
    validateDevice(identity, product) {
        if (identity.model.replace(/^ota-/, '') !== product.split('_')[0])
            throw new InstallError('model', `Device ${identity.text} does not match ${product}`);
        if (identity.loader && !/^ota-FM-1_9\d\d$/i.test(identity.text))
            throw new InstallError('foreign', `Foreign update loader: ${identity.text}`);
    }
    async serve(link, image, finish, stage) {
        const end = this.clock.now() + this.timeouts[`${stage}Total`];
        let requests = 0;
        this.progress(stage, {requests, total: image.length});
        link.send(UPGRADE);
        while (true) {
            if (this.clock.now() >= end) throw new InstallError('timeout', 'Update session deadline exceeded');
            const idleEnd = this.clock.now() + this.timeouts[stage];
            let r;
            do {
                if (this.clock.now() >= Math.min(end, idleEnd))
                    throw new InstallError('timeout', 'Device response timed out');
                r = parseRequest(await link.inbox.next(Math.min(end, idleEnd) - this.clock.now()));
            } while (!r);
            const {flags, address, length} = r;
            if (address === FINISH_CHECK || address === FINISH_WRITE) {
                link.send(response(address, encoder.encode('success\0')));
                if (address === finish) return;
                continue;
            }
            if (length > MAX_DATA || address + length > image.length)
                throw new InstallError('badreq', `Device requested ${address}+${length} outside the package`);
            link.send(response(address, image.slice(address, address + length), flags));
            this.progress(stage, {requests: ++requests, end: address + length, total: image.length});
        }
    }
    pairs() {
        const named = /mc-1|fm-1|orbit|felucca|sloop|clipper|ota|composite|sinco|usb-midi/i;
        const connected = port => port.state === 'connected';
        const outs = [...this.access.outputs.values()].filter(connected);
        const base = name => (name || '').replace(/\s+\d+$/, '');
        const pairs = [...this.access.inputs.values()].filter(p => connected(p) && named.test(p.name || '')).flatMap(input => {
            const exact = outs.filter(o => o.name === input.name);
            const same = exact.length ? exact : outs.filter(o => base(o.name) === base(input.name));
            return same.length === 1 ? [[input, same[0]]] : [];
        });
        if (pairs.length > 1) throw new InstallError('ambiguous', 'Multiple update devices: keep only the selected FM-1 connected');
        return pairs;
    }
    async reconnect(want, ms, code) {
        const end = this.clock.now() + ms;
        // Snapshot existing ports once, then probe again only after actual state events.
        while (true) {
            if (this.clock.now() >= end) throw new InstallError(code, 'Device did not reappear');
            for (const [input, output] of this.pairs()) {
                let link;
                let accepted = false;
                try {
                    link = await this.open(input, output, undefined, Math.min(this.timeouts.open, end - this.clock.now()));
                    const identity = await this.handshake(link, undefined, end);
                    if (want(identity)) {
                        accepted = true;
                        return {link, identity};
                    }
                } catch (error) {
                    if (!['lost', 'timeout'].includes(error.code)) throw error;
                } finally {
                    if (link && !accepted) await link.close();
                }
            }
            try { await this.changes.next(end - this.clock.now()); }
            catch (error) {
                if (error.code === 'timeout') throw new InstallError(code, 'Device did not reappear');
                throw error;
            }
        }
    }
    async install(raw, {sha256, signal} = {}) {
        if (this.running) throw new InstallError('busy', 'An installation is already running');
        this.running = true;
        this.unsafe = false;
        let link;
        const states = new Map();
        const state = event => {
            const port = event.port;
            if (states.get(port) !== port.state) {
                states.set(port, port.state);
                this.changes.push(true);
            }
        };
        try {
            this.progress('package');
            const pkg = await loadPackage(raw, {sha256});
            if (signal?.aborted) throw new InstallError('cancelled', 'Cancelled before upgrade');
            if (!this.access?.sysexEnabled || !this.input || !this.output)
                throw new InstallError('permission', 'Select input/output with SysEx permission first');
            this.changes = new Inbox(this.clock);
            for (const port of [...this.access.inputs.values(), ...this.access.outputs.values()])
                states.set(port, port.state);
            this.access.addEventListener('statechange', state);
            this.progress('identify');
            link = await this.open(this.input, this.output, signal);
            const initial = await this.handshake(link, signal);
            this.validateDevice(initial, pkg.product);
            this.progress('ready', {identity: initial.text});
            if (signal?.aborted) throw new InstallError('cancelled', 'Cancelled before upgrade');
            this.unsafe = true;
            if (!initial.loader) {
                await this.serve(link, pkg.image, FINISH_CHECK, 'check');
                await link.close();
                link = null;
                this.progress('loader');
                const found = await this.reconnect(identity => {
                    if (!identity.loader) return false;
                    this.validateDevice(identity, pkg.product);
                    return true;
                }, this.timeouts.loader, 'noloader');
                link = found.link;
            }
            await this.serve(link, pkg.image, FINISH_WRITE, 'write');
            await link.close();
            link = null;
            this.progress('reboot');
            const back = await this.reconnect(identity => !identity.loader, this.timeouts.reboot, 'noreturn');
            link = back.link;
            if (back.identity.text !== pkg.product)
                throw new InstallError('mismatch', `Written, but device reports ${back.identity.text}; expected ${pkg.product}`);
            this.progress('done', {identity: back.identity.text});
            return {identity: back.identity.text, product: pkg.product};
        } catch (error) {
            const failure = error instanceof InstallError ? error : new InstallError('io', error.message);
            failure.recoveryRequired = this.unsafe;
            this.progress('error', {code: failure.code, recoveryRequired: failure.recoveryRequired});
            throw failure;
        } finally {
            this.access?.removeEventListener('statechange', state);
            if (link) await link.close();
            this.running = false;
        }
    }
}

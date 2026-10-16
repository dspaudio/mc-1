// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {FirmwareInstaller as ProtocolInstaller} from '../site/firmware-ota.js';

// Product-site UI boundary tests; protocol and package codecs have separate tests.
const source = fs.readFileSync('site/firmware-install.js', 'utf8')
    .replace(/^import .+;$/m, '')
    .replace('void fetchRelease();', 'globalThis.boot = fetchRelease();');
const manifest = {version: '0.3.1', identity: 'FM-1_900', file: 'mc-1-0.3.1.fwsc',
    bytes: 960, sha256: 'a'.repeat(64)};
const deferred = () => {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return {promise, resolve, reject};
};
function element() {
    return {value: '', disabled: false, hidden: false, textContent: '', dataset: {},
        attributes: {}, handlers: {},
        addEventListener(name, handler) { this.handlers[name] = handler; },
        setAttribute(name, value) { this.attributes[name] = value; },
        removeAttribute(name) { delete this.attributes[name]; },
        replaceChildren(...options) { this.options = options; this.value = options[0]?.value || ''; }};
}
function fixture(initial = 'ready') {
    let mode = initial, requests = 0, starts = 0, opens = 0, closes = 0;
    const elements = new Map();
    const get = id => {
        if (!elements.has(id)) elements.set(id, element());
        return elements.get(id);
    };
    const input = {id: 'in', name: 'Felucca', state: 'connected'};
    const output = {id: 'out', name: 'Felucca', state: 'connected'};
    const access = {sysexEnabled: true, inputs: new Map([['in', input]]),
        outputs: new Map([['out', output]]), listeners: new Set(),
        addEventListener(name, listener) { assert.equal(name, 'statechange'); this.listeners.add(listener); }};
    if (mode === 'mc-1-name') input.name = output.name = 'MC-1';
    if (mode === 'no-device') { access.inputs.clear(); access.outputs.clear(); }
    if (mode === 'ambiguous') {
        access.inputs.set('other-in', {id: 'other-in', name: 'FM-1 Other', state: 'connected'});
        access.outputs.set('other-out', {id: 'other-out', name: 'FM-1 Other', state: 'connected'});
    }
    if (mode === 'unmatched') output.name = 'Unrelated Synth';
    const started = deferred(), finished = deferred();
    const windowHandlers = {};
    const sandbox = {
        console, Uint8Array, AbortSignal, Error, Number, Boolean, Option: class {
            constructor(text, value) { Object.assign(this, {text, value}); }
        },
        isSecureContext: mode !== 'insecure',
        document: {getElementById: get},
        window: {addEventListener(name, listener) { windowHandlers[name] = listener; }},
        navigator: mode === 'unsupported' ? {} : {
            async requestMIDIAccess(options) {
                requests++;
                assert.equal(options.sysex, true);
                if (mode === 'denied') throw new Error('DENIED_SENTINEL');
                if (mode === 'no-sysex') return {...access, sysexEnabled: false};
                return access;
            },
        },
        async fetch(url, options) {
            assert(options.signal instanceof AbortSignal);
            if (url === 'firmware-release.json') return {
                ok: mode !== 'http', status: 503,
                json: async () => ({...manifest, ...(mode === 'bad-metadata' ? {file: '../outside.fwsc'} : {})}),
            };
            assert.equal(url, manifest.file);
            return {ok: true, arrayBuffer: async () => new Uint8Array(mode === 'short' ? 959 : 960).buffer};
        },
        async loadPackage(bytes, options) {
            assert.equal(bytes.length, manifest.bytes);
            assert.equal(options.sha256, manifest.sha256);
            if (mode === 'checksum') throw new Error('CHECKSUM_SENTINEL');
            return {product: manifest.identity};
        },
        FirmwareInstaller: class {
            constructor(options) { Object.assign(this, options); }
            pairs() { return ProtocolInstaller.prototype.pairs.call(this); }
            async open(incoming, outgoing) {
                opens++;
                assert.equal(incoming, input); assert.equal(outgoing, output);
                return {async close() { closes++; }};
            }
            async handshake() {
                return {text: mode === 'wrong-model' ? 'OTHER_001' : manifest.identity, loader: false};
            }
            validateDevice(found, expected) {
                assert.equal(expected, manifest.identity);
                if (found.text !== expected) throw new Error('MODEL_SENTINEL');
            }
            async install(bytes, options) {
                starts++;
                assert.equal(bytes.length, manifest.bytes);
                assert.equal(options.sha256, manifest.sha256);
                this.onProgress({stage: 'write', end: 480, total: 960});
                started.resolve();
                await finished.promise;
                if (mode === 'recovery') throw Object.assign(new Error('RECOVERY_SENTINEL'), {recoveryRequired: true});
                input.state = output.state = 'disconnected';
                access.inputs = new Map(); access.outputs = new Map();
                this.onProgress({stage: 'done', identity: manifest.identity});
                return {identity: manifest.identity};
            }
        },
    };
    vm.runInNewContext(source, sandbox);
    return {get, access, input, output, started, finished, windowHandlers,
        boot: sandbox.boot, change(value) { mode = value; },
        counts: () => ({requests, starts, opens, closes}),
        click: id => get(id).handlers.click(),
    };
}

for (const mode of ['unsupported', 'insecure']) {
    const f = fixture(mode); await f.boot;
    assert(f.get('connect-device').disabled);
    assert(f.get('install-firmware').disabled);
    assert.equal(f.get('install-status').dataset.state, 'error');
    assert.equal(f.counts().requests, 0);
}
for (const mode of ['http', 'bad-metadata', 'short', 'checksum']) {
    const f = fixture(mode); await f.boot;
    assert(f.get('connect-device').disabled);
    assert(f.get('install-firmware').disabled);
    assert.equal(f.get('package-status').attributes.role, 'alert');
    assert.equal(f.counts().starts, 0);
}
for (const mode of ['denied', 'no-sysex']) {
    const f = fixture(mode); await f.boot; await f.click('connect-device');
    assert(f.get('install-firmware').disabled);
    assert.equal(f.get('install-status').dataset.state, 'error');
    f.change('ready'); await f.click('connect-device');
    assert.equal(f.get('install-firmware').disabled, false);
    assert.equal(f.counts().requests, 2);
    assert.equal(f.counts().opens, f.counts().closes);
}
let f = fixture('wrong-model'); await f.boot; await f.click('connect-device');
assert(f.get('install-firmware').disabled);
assert.equal(f.get('install-status').attributes.role, 'alert');
for (const mode of ['no-device', 'ambiguous', 'unmatched']) {
    const missing = fixture(mode); await missing.boot; await missing.click('connect-device');
    assert(missing.get('install-firmware').disabled);
    assert.equal(missing.get('install-status').attributes.role, 'alert');
    assert.equal(missing.counts().opens, 0);
    assert.equal(missing.counts().starts, 0);
}
{
    const named = fixture('mc-1-name'); await named.boot; await named.click('connect-device');
    assert.equal(named.get('install-firmware').disabled, false);
    assert.equal(named.counts().starts, 0);
    assert.equal(named.counts().opens, named.counts().closes);
}
f = fixture(); await f.boot;
assert.equal(f.counts().requests, 0); assert.equal(f.counts().starts, 0);
assert.equal(f.get('package-hash').textContent, manifest.sha256);
await f.click('connect-device');
assert.equal(f.counts().starts, 0);
assert.equal(f.get('install-firmware').disabled, false);
f.input.state = 'disconnected';
for (const listener of f.access.listeners) listener({port: f.input});
assert(f.get('install-firmware').disabled);

for (const mode of ['ready', 'recovery']) {
    f = fixture(mode); await f.boot; await f.click('connect-device');
    const running = f.click('install-firmware');
    await f.started.promise;
    assert.equal(f.counts().starts, 1);
    assert(f.get('connect-device').disabled); assert(f.get('install-firmware').disabled);
    assert.equal(f.get('install-progress').hidden, false);
    assert.equal(f.get('install-progress').value, 50);
    let prevented = false;
    const event = {preventDefault() { prevented = true; }};
    f.windowHandlers.beforeunload(event);
    assert(prevented); assert.equal(event.returnValue, '');
    f.finished.resolve(); await running;
    assert.equal(f.get('connect-device').disabled, false);
    assert(f.get('install-firmware').disabled);
    assert.equal(f.get('install-status').dataset.state, mode === 'ready' ? 'verified' : 'error');
    assert.equal(f.get('install-status').attributes.role, mode === 'ready' ? 'status' : 'alert');
    prevented = false; f.windowHandlers.beforeunload(event); assert.equal(prevented, false);
}
console.log(JSON.stringify({firmwareInstallUI: 'PASS', hardwareAccess: false,
    scenarios: ['unsupported', 'insecure', 'metadata HTTP', 'invalid metadata', 'short package',
        'checksum rejection', 'permission retry', 'SysEx retry', 'model rejection',
        'no automatic install', 'unplug', 'progress', 'page retention', 'verified after re-enumeration', 'recovery']}));

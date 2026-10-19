// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {FirmwareInstaller as ProtocolInstaller} from '../site/firmware-ota.js';

// Product-site UI boundary tests; protocol and package codecs have separate tests.
const source = fs.readFileSync('site/firmware-install.js', 'utf8')
    .replace(/^import .+;$/gm, '')
    .replace('void fetchRelease();', 'globalThis.boot = fetchRelease();');
const original = {version: '1.0.2', core_mode: 'single', identity: 'FM-1_900',
    file: 'mc-1-1.0.2.fwsc', bytes: 610089,
    sha256: 'a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5'};
const manifest = {version: '1.1.0', core_mode: 'dual', identity: 'FM-1_900', file: 'mc-1-1.1.0.fwsc',
    bytes: 960, sha256: 'a'.repeat(64), alternatives: [original]};
const deferred = () => {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return {promise, resolve, reject};
};
function element() {
    return {value: '', files: [], checked: false, disabled: false, hidden: false, textContent: '', dataset: {},
        attributes: {}, handlers: {},
        addEventListener(name, handler) { this.handlers[name] = handler; },
        setAttribute(name, value) { this.attributes[name] = value; },
        removeAttribute(name) { delete this.attributes[name]; },
        replaceChildren(...options) { this.options = options; this.value = options[0]?.value || ''; }};
}
function fixture(initial = 'ready', metadata = manifest) {
    let mode = initial, requests = 0, starts = 0, opens = 0, closes = 0;
    const records = [manifest, original], verified = new Map(), downloads = [];
    const fetchQueue = [], verifyQueue = [], identityQueue = [];
    const preparations = [], preparationQueue = [], installs = [];
    const stockRaw = new Uint8Array(1024), stockHash = 'b'.repeat(64);
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
                json: async () => ({...metadata, ...(mode === 'bad-metadata' ? {file: '../outside.fwsc'} : {})}),
            };
            const record = records.find(value => value.file === url);
            assert(record);
            downloads.push(url);
            const gate = fetchQueue.shift();
            if (gate) { gate.started.resolve(); await gate.finished.promise; }
            return {ok: true, arrayBuffer: async () => {
                const bytes = new Uint8Array(mode === 'short' ? 959 : record.bytes);
                bytes[0] = record === original ? 2 : 11;
                return bytes.buffer;
            }};
        },
        async loadPackage(bytes, options) {
            const record = bytes[0] === 2 ? original : manifest;
            assert.equal(bytes.length, record.bytes);
            assert.equal(options.sha256, record.sha256);
            const gate = verifyQueue.shift();
            if (gate) { gate.started.resolve(); await gate.finished.promise; }
            if (mode === 'checksum') throw new Error('CHECKSUM_SENTINEL');
            verified.set(bytes, record);
            return {product: record.identity};
        },
        async prepareStockRestore(bytes, loaderBytes, options) {
            assert(verified.has(loaderBytes));
            assert.equal(options.loaderSha256, verified.get(loaderBytes).sha256);
            preparations.push(bytes);
            const gate = preparationQueue.shift();
            if (gate) { gate.started.resolve(); await gate.finished.promise; }
            if (bytes[0] !== 15) throw new Error('STOCK_SENTINEL');
            return {raw: stockRaw, sha256: stockHash, product: 'FM-1_015'};
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
                const gate = identityQueue.shift();
                if (gate) { gate.started.resolve(); await gate.finished.promise; }
                return {text: mode === 'wrong-model' ? 'OTHER_001' : mode === 'loader' ? 'ota-FM-1_900' : manifest.identity,
                    model: mode === 'wrong-model' ? 'OTHER' : mode === 'loader' ? 'ota-FM-1' : 'FM-1',
                    loader: mode === 'loader'};
            }
            validateDevice(found, expected) {
                assert.equal(expected, manifest.identity);
                ProtocolInstaller.prototype.validateDevice.call(this, found, expected);
            }
            async install(bytes, options) {
                starts++;
                const stock = bytes === stockRaw;
                if (!stock) assert(verified.has(bytes));
                assert.equal(options.sha256, stock ? stockHash : verified.get(bytes).sha256);
                installs.push({bytes, sha256: options.sha256});
                this.onProgress({stage: 'write', end: 480, total: 960});
                started.resolve();
                await finished.promise;
                if (mode === 'recovery') throw Object.assign(new Error('RECOVERY_SENTINEL'), {recoveryRequired: true});
                input.state = output.state = 'disconnected';
                access.inputs = new Map(); access.outputs = new Map();
                const identity = stock ? 'FM-1_015' : manifest.identity;
                this.onProgress({stage: 'done', identity});
                return {identity};
            }
        },
    };
    vm.runInNewContext(source, sandbox);
    return {get, access, input, output, started, finished, windowHandlers,
        preparations, installs, stockRaw, stockHash, downloads,
        queueFetch() {
            const gate = {started: deferred(), finished: deferred()};
            fetchQueue.push(gate);
            return gate;
        },
        queueVerification() {
            const gate = {started: deferred(), finished: deferred()};
            verifyQueue.push(gate);
            return gate;
        },
        queueIdentity() {
            const gate = {started: deferred(), finished: deferred()};
            identityQueue.push(gate);
            return gate;
        },
        choose(version) {
            get('firmware-version').value = version;
            return get('firmware-version').handlers.change();
        },
        queuePreparation() {
            const gate = {started: deferred(), finished: deferred()};
            preparationQueue.push(gate);
            return gate;
        },
        select(file) {
            get('restore-file').files = file ? [file] : [];
            return get('restore-file').handlers.change();
        },
        acknowledge(value = true) {
            get('restore-ack').checked = value;
            get('restore-ack').handlers.change();
        },
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
    for (const id of ['restore-file', 'restore-ack', 'restore-firmware']) assert(f.get(id).disabled);
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
const stockFile = (marker = 15) => ({arrayBuffer: async () => new Uint8Array([marker]).buffer});
for (const mode of ['ready', 'loader', 'recovery']) {
    f = fixture(mode); await f.boot;
    assert(f.get('restore-firmware').disabled);
    await f.select(stockFile());
    assert.equal(f.get('restore-status').dataset.state, 'verified');
    assert.equal(f.counts().requests, 0); assert.equal(f.counts().starts, 0);
    assert.equal(f.get('restore-ack').disabled, false);
    f.acknowledge();
    assert(f.get('restore-firmware').disabled);
    await f.click('connect-device');
    assert.equal(f.get('restore-firmware').disabled, false);
    f.acknowledge(false);
    await f.click('restore-firmware');
    assert.equal(f.counts().starts, 0);
    f.acknowledge();
    const running = f.click('restore-firmware');
    await f.started.promise;
    assert.equal(f.installs[0].bytes, f.stockRaw);
    assert.equal(f.installs[0].sha256, f.stockHash);
    assert.equal(f.get('restore-progress').hidden, false);
    assert.equal(f.get('restore-progress').value, 50);
    assert.equal(f.get('install-progress').hidden, true);
    for (const id of ['connect-device', 'install-firmware', 'restore-file', 'restore-ack', 'restore-firmware'])
        assert(f.get(id).disabled);
    await f.click('install-firmware'); await f.click('connect-device'); await f.click('restore-firmware');
    assert.equal(f.counts().starts, 1); assert.equal(f.counts().requests, 1);
    let prevented = false;
    f.windowHandlers.beforeunload({preventDefault() { prevented = true; }});
    assert(prevented);
    f.finished.resolve(); await running;
    assert.equal(f.get('install-status').dataset.state, mode === 'recovery' ? 'error' : 'verified');
    assert.equal(f.get('restore-status').dataset.state, mode === 'recovery' ? 'error' : 'verified');
    assert.equal(f.get('restore-file').disabled, false);
    assert(f.get('restore-firmware').disabled);
    prevented = false;
    f.windowHandlers.beforeunload({preventDefault() { prevented = true; }});
    assert.equal(prevented, false);
    if (mode === 'recovery') {
        f.change('loader'); await f.click('connect-device');
        assert.equal(f.get('restore-firmware').disabled, false);
        await f.click('restore-firmware');
        assert.equal(f.counts().starts, 2);
        assert.equal(f.get('install-status').dataset.state, 'verified');
    } else {
        assert.equal(f.get('device-identity').textContent, 'FM-1_015');
    }
}
{
    f = fixture(); await f.boot; await f.click('connect-device');
    await f.select(stockFile()); f.acknowledge();
    const gate = f.queuePreparation();
    const old = f.select(stockFile());
    await gate.started.promise;
    assert(f.get('restore-firmware').disabled);
    assert.equal(f.get('restore-ack').checked, false);
    await f.select(stockFile(0));
    assert.equal(f.get('restore-status').attributes.role, 'alert');
    gate.finished.resolve(); await old;
    assert.equal(f.get('restore-status').attributes.role, 'alert');
    assert(f.get('restore-firmware').disabled); assert(f.get('restore-ack').disabled);
    await f.click('restore-firmware'); assert.equal(f.counts().starts, 0);
}
{
    f = fixture(); await f.boot; await f.click('connect-device');
    const gate = f.queuePreparation();
    const old = f.select(stockFile(0));
    await gate.started.promise;
    await f.select(stockFile()); f.acknowledge();
    gate.finished.resolve(); await old;
    assert.equal(f.get('restore-status').dataset.state, 'verified');
    assert.equal(f.get('restore-firmware').disabled, false);
    await f.select(null);
    assert(f.get('restore-firmware').disabled); assert(f.get('restore-ack').disabled);
}
{
    f = fixture(); await f.boot; await f.click('connect-device');
    const read = deferred();
    const old = f.select({arrayBuffer: () => read.promise});
    await f.select(stockFile()); f.acknowledge();
    read.resolve(new Uint8Array([0]).buffer); await old;
    assert.equal(f.preparations.length, 1);
    assert.equal(f.get('restore-firmware').disabled, false);
    f.input.state = 'disconnected';
    for (const listener of f.access.listeners) listener({port: f.input});
    assert(f.get('restore-firmware').disabled);
}
{
    const f = fixture(); await f.boot;
    assert.deepEqual(f.get('firmware-version').options.map(option => option.value), ['1.1.0', '1.0.2']);
    assert.equal(f.get('firmware-version').value, '1.1.0');
    assert.equal(f.get('package-download').href, manifest.file);
    await f.choose('1.0.2');
    assert.equal(f.get('package-hash').textContent, original.sha256);
    assert.equal(f.get('package-download').href, original.file);
    assert.equal(f.get('package-download').download, original.file);
    assert.equal(f.get('release-link').href, 'https://github.com/dspaudio/mc-1/releases/tag/v1.0.2');
    assert.equal(f.get('package-size').textContent, '610,089 bytes');
    assert.equal(f.get('package-version').textContent.includes(original.version), true);
    assert.equal(f.get('install-firmware').textContent.includes(original.version), true);
    await f.click('connect-device');
    const running = f.click('install-firmware');
    await f.started.promise;
    assert(f.get('firmware-version').disabled);
    await f.choose('1.1.0');
    assert.equal(f.get('firmware-version').value, original.version);
    assert.equal(f.get('package-hash').textContent, original.sha256);
    assert.equal(f.installs[0].sha256, original.sha256);
    assert.equal(f.installs[0].bytes.length, original.bytes);
    assert.equal(f.installs[0].bytes[0], 2);
    f.finished.resolve(); await running;
    assert.equal(f.get('firmware-version').disabled, false);
}
{
    const f = fixture(); await f.boot;
    const gate = f.queueIdentity();
    const connecting = f.click('connect-device');
    await gate.started.promise;
    assert(f.get('firmware-version').disabled);
    await f.choose('1.0.2');
    assert.equal(f.get('firmware-version').value, manifest.version);
    gate.finished.resolve(); await connecting;
    assert.equal(f.get('firmware-version').disabled, false);
}
{
    const f = fixture(); await f.boot;
    await f.choose('1.0.2'); await f.select(stockFile()); f.acknowledge();
    await f.click('connect-device');
    const running = f.click('restore-firmware');
    await f.started.promise;
    assert(f.get('firmware-version').disabled);
    await f.choose('1.1.0');
    assert.equal(f.get('firmware-version').value, original.version);
    assert.equal(f.installs[0].sha256, f.stockHash);
    f.finished.resolve(); await running;
}
{
    const f = fixture(); await f.boot;
    const gate = f.queueFetch();
    const old = f.choose('1.0.2');
    await gate.started.promise;
    assert(f.get('connect-device').disabled);
    assert(f.get('restore-file').disabled);
    await f.choose('1.1.0');
    gate.finished.resolve(); await old;
    assert.equal(f.get('package-hash').textContent, manifest.sha256);
    assert.equal(f.get('package-download').href, manifest.file);
    assert.equal(f.get('connect-device').disabled, false);
}
for (const fails of [false, true]) {
    const f = fixture(); await f.boot;
    const gate = f.queueVerification();
    const old = f.choose('1.0.2');
    await gate.started.promise;
    await f.choose('1.1.0');
    if (fails) gate.finished.reject(new Error('STALE_VERIFICATION_SENTINEL'));
    else gate.finished.resolve();
    await old;
    assert.equal(f.get('package-hash').textContent, manifest.sha256);
    assert.equal(f.get('package-status').attributes.role, 'status');
    await f.click('connect-device');
    const running = f.click('install-firmware');
    await f.started.promise;
    assert.equal(f.installs[0].sha256, manifest.sha256);
    assert.equal(f.installs[0].bytes[0], 11);
    f.finished.resolve(); await running;
}
for (const fails of [false, true]) {
    const f = fixture(); await f.boot; await f.click('connect-device');
    const gate = f.queuePreparation();
    const old = f.select(stockFile());
    await gate.started.promise;
    await f.choose('1.0.2');
    if (fails) gate.finished.reject(new Error('STALE_RESTORE_SENTINEL'));
    else gate.finished.resolve();
    await old;
    assert(f.get('restore-firmware').disabled);
    assert(f.get('restore-ack').disabled);
    assert.equal(f.get('restore-ack').checked, false);
    assert.equal(f.get('restore-status').dataset.state, '');
    await f.select(stockFile()); f.acknowledge();
    assert.equal(f.get('restore-firmware').disabled, false);
}
{
    const f = fixture(); await f.boot; await f.click('connect-device');
    const gate = f.queuePreparation();
    const old = f.select(stockFile());
    await gate.started.promise;
    const running = f.click('install-firmware');
    await f.started.promise;
    gate.finished.resolve(); await old;
    f.finished.resolve(); await running;
    assert(f.get('restore-ack').disabled);
    assert(f.get('restore-firmware').disabled);
}
const invalidAlternatives = [null, {}, [], [original, original], [null],
    ...Object.keys(original).map(key => [{...original, [key]: null}]),
    [{...original, file: '../outside.fwsc'}], [{...original, bytes: 1048577}],
    [{...original, bytes: 959}], [{...original, bytes: 610089.5}],
    [{...original, sha256: 'b'.repeat(64)}]];
for (const alternatives of invalidAlternatives) {
    const f = fixture('ready', {...manifest, alternatives}); await f.boot;
    assert(f.get('connect-device').disabled);
    assert(f.get('install-firmware').disabled);
    assert.equal(f.get('package-status').attributes.role, 'alert');
    assert.equal(f.downloads.length, 0);
}
{
    const f = fixture('ready', {...manifest, version: '1.0.2', file: original.file});
    await f.boot;
    assert.equal(f.get('package-status').attributes.role, 'alert');
    assert.equal(f.downloads.length, 0);
}
console.log(JSON.stringify({firmwareInstallUI: 'PASS', hardwareAccess: false,
    scenarios: ['unsupported', 'insecure', 'metadata HTTP', 'invalid metadata', 'short package',
        'checksum rejection', 'permission retry', 'SysEx retry', 'model rejection',
        'no automatic install', 'unplug', 'progress', 'page retention', 'verified after re-enumeration', 'recovery',
        'V15 acknowledgement', 'V15 verified loader input', 'V15 transfer lock', 'V15 loader resume',
        'V15 recovery retry', 'V15 validation rejection', 'V15 stale validation success',
        'V15 stale validation failure', 'V15 stale file read', 'V15 selection cleared',
        '1.0.2 package and SHA', 'selected metadata links', 'connection selection lock',
        'install selection lock', 'restore selection lock', 'stale package fetch',
        'stale package verification success/failure', 'V15 version-change invalidation',
        'V15 transfer invalidation', 'alternative boundaries and uniqueness']}));

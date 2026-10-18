// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 dspaudio
// Firmware installation belongs to the MC-1 product site.
import {FirmwareInstaller, loadPackage} from './firmware-ota.js';
import {prepareStockRestore} from './firmware-stock.js';

const get = id => document.getElementById(id);
const connect = get('connect-device');
const install = get('install-firmware');
const status = get('install-status');
const progress = get('install-progress');
const restoreFile = get('restore-file');
const restoreAck = get('restore-ack');
const restore = get('restore-firmware');
const restoreStatus = get('restore-status');
const restoreProgress = get('restore-progress');
let access, release, packageBytes, identity, deviceInput, deviceOutput;
let busy = false, installing = false, restoring = false;
let selection = 0;
let fileSelection = 0, preparedRestore;

function message(text, state = '') {
    status.textContent = text;
    status.dataset.state = state;
    status.setAttribute('role', state === 'error' ? 'alert' : 'status');
    if (restoring) restoreMessage(text, state);
}

function controls() {
    connect.disabled = busy || !packageBytes || !navigator.requestMIDIAccess || !isSecureContext;
    install.disabled = busy || !packageBytes || !identity;
    restoreFile.disabled = busy || !packageBytes;
    restoreAck.disabled = busy || !preparedRestore;
    restore.disabled = busy || !identity || !preparedRestore || !restoreAck.checked;
}

function restoreMessage(text, state = '') {
    restoreStatus.textContent = text;
    restoreStatus.dataset.state = state;
    restoreStatus.setAttribute('role', state === 'error' ? 'alert' : 'status');
}

restoreFile.addEventListener('change', async () => {
    if (busy) return;
    const current = ++fileSelection;
    const file = restoreFile.files[0];
    preparedRestore = undefined;
    restoreAck.checked = false;
    controls();
    if (!file || !packageBytes) {
        restoreMessage('Select the unmodified official FM-1 V15 .fwsc file.');
        return;
    }
    restoreMessage('Verifying official V15 firmware and preparing the safe update loader…');
    try {
        const bytes = new Uint8Array(await file.arrayBuffer());
        if (current !== fileSelection) return;
        const prepared = await prepareStockRestore(bytes, packageBytes, {loaderSha256: release.sha256});
        if (current !== fileSelection) return;
        preparedRestore = prepared;
        restoreMessage('Official V15 verified. Acknowledge the project and patch limitations, then connect and restore.', 'verified');
    } catch (error) {
        if (current !== fileSelection) return;
        restoreMessage(`V15 verification failed: ${error.message}. Select the official file to retry.`, 'error');
    } finally {
        controls();
    }
});
restoreAck.addEventListener('change', controls);

function selected() {
    return [deviceInput, deviceOutput];
}

function ports({preserveStatus = false} = {}) {
    if (installing) return;
    if (deviceInput && (deviceInput.state !== 'connected' || deviceOutput.state !== 'connected' ||
            access.inputs.get(deviceInput.id) !== deviceInput || access.outputs.get(deviceOutput.id) !== deviceOutput)) {
        selection++;
        identity = null;
        deviceInput = deviceOutput = undefined;
        if (!preserveStatus) {
            get('device-identity').textContent = 'FM-1 disconnected.';
            message('Reconnect the instrument, then click Connect FM-1.');
        }
    }
    controls();
}

async function identify() {
    const [incoming, outgoing] = selected();
    identity = null;
    const current = ++selection;
    if (!incoming || !outgoing || !release) {
        controls();
        return;
    }
    busy = true;
    controls();
    message('Identifying the selected instrument…');
    let link;
    const probe = new FirmwareInstaller({access, input: incoming, output: outgoing});
    try {
        link = await probe.open(incoming, outgoing);
        const found = await probe.handshake(link);
        probe.validateDevice(found, release.identity);
        if (current !== selection || incoming.state !== 'connected' || outgoing.state !== 'connected') return;
        identity = found;
        get('device-identity').textContent = `Identified ${found.text}${found.loader ? ' — update loader ready' : ''}.`;
        message(`Ready to install MC-1 ${release.version} on ${found.text}.`);
    } catch (error) {
        if (current === selection) {
            get('device-identity').textContent = 'Device not identified.';
            message(`${error.message}. Check USB, then click Connect FM-1 to retry.`, 'error');
        }
    } finally {
        if (link) await link.close();
        busy = false;
        controls();
    }
}

connect.addEventListener('click', async () => {
    if (busy || !packageBytes) return;
    busy = true;
    controls();
    try {
        if (!access) {
            const granted = await navigator.requestMIDIAccess({sysex: true});
            if (!granted.sysexEnabled) throw new Error('SysEx permission was not granted');
            access = granted;
            access.addEventListener('statechange', ports);
        }
        ports();
        identity = null;
        deviceInput = deviceOutput = undefined;
        const pairs = new FirmwareInstaller({access}).pairs();
        if (pairs.length) {
            [deviceInput, deviceOutput] = pairs[0];
            message('FM-1 found. Reading its identity…');
        } else {
            get('device-identity').textContent = 'No FM-1 found.';
            message('No FM-1 MIDI connection found. Connect one instrument directly by USB and retry.', 'error');
        }
    } catch (error) {
        message(`MIDI connection failed: ${error.message}. Allow MIDI SysEx access and retry.`, 'error');
    } finally {
        busy = false;
        controls();
    }
    if (selected().every(Boolean)) await identify();
});

const stages = {
    package: 'Verifying package', identify: 'Identifying instrument',
    ready: 'Instrument identified', check: 'Checking package and staging loader',
    loader: 'Waiting for update loader', write: 'Writing firmware',
    reboot: 'Waiting for restart', done: 'Restart verified',
};
function onProgress(event) {
    if (event.stage === 'error') return;
    const activeProgress = restoring ? restoreProgress : progress;
    let text = stages[event.stage] || event.stage;
    if (event.stage === 'write' && event.total) {
        const percent = Math.min(100, Math.floor((event.end || 0) * 100 / event.total));
        activeProgress.value = percent;
        text += ` — ${percent}%`;
    } else if (event.stage === 'done') activeProgress.value = 100;
    else activeProgress.removeAttribute('value');
    if (event.identity) get('device-identity').textContent = event.identity;
    message(`${text}. Keep USB and power connected.`);
}

async function transfer(stock = false) {
    if (busy || !packageBytes || !identity) return;
    if (stock && (!preparedRestore || !restoreAck.checked)) return;
    const prepared = stock ? preparedRestore : {raw: packageBytes, sha256: release.sha256};
    const [incoming, outgoing] = selected();
    if (!incoming || !outgoing) return;
    busy = installing = true;
    restoring = stock;
    if (!stock && preparedRestore)
        restoreMessage('Official V15 file verified and ready for a future restore.', 'verified');
    controls();
    progress.hidden = stock;
    restoreProgress.hidden = !stock;
    const updater = new FirmwareInstaller({access, input: incoming, output: outgoing, onProgress});
    try {
        const result = await updater.install(prepared.raw, {sha256: prepared.sha256});
        message(stock
            ? `Official V15 restored. Restart verified: ${result.identity}.`
            : `MC-1 ${release.version} package installed. Restart verified: ${result.identity}. ` +
                'The protocol identity does not report the MC-1 version.', 'verified');
    } catch (error) {
        message(error.recoveryRequired
            ? `Update needs recovery: ${error.message}. Keep the instrument powered, reconnect its update-loader ports, and retry this same ${stock ? 'V15 restore' : 'package'}.`
            : `Installation did not start: ${error.message}. Reconnect the selected instrument and retry.`, 'error');
    } finally {
        busy = installing = restoring = false;
        identity = null;
        ports({preserveStatus: true});
        controls();
    }
}
install.addEventListener('click', () => transfer());
restore.addEventListener('click', () => transfer(true));

window.addEventListener('beforeunload', event => {
    if (!installing) return;
    event.preventDefault();
    event.returnValue = '';
});

async function fetchRelease() {
    try {
        const metadata = await fetch('firmware-release.json', {cache: 'no-cache', signal: AbortSignal.timeout(20000)});
        if (!metadata.ok) throw new Error(`Release metadata HTTP ${metadata.status}`);
        const value = await metadata.json();
        if (!/^\d+\.\d+\.\d+$/.test(value.version) || value.identity !== 'FM-1_900' ||
            !/^[a-f0-9]{64}$/.test(value.sha256) || !/^[a-zA-Z0-9._-]+\.fwsc$/.test(value.file) ||
            !Number.isInteger(value.bytes) || value.bytes < 960 || value.bytes > 1048576)
            throw new Error('Invalid release metadata');
        const response = await fetch(value.file, {signal: AbortSignal.timeout(20000)});
        if (!response.ok) throw new Error(`Firmware package HTTP ${response.status}`);
        const bytes = new Uint8Array(await response.arrayBuffer());
        if (bytes.length !== value.bytes) throw new Error('Package size mismatch');
        const pkg = await loadPackage(bytes, {sha256: value.sha256});
        if (pkg.product !== value.identity) throw new Error('Package identity mismatch');
        release = value;
        packageBytes = bytes;
        get('package-version').textContent = `MC-1 ${value.version}`;
        get('package-identity').textContent = value.identity;
        get('package-size').textContent = `${value.bytes.toLocaleString('en-US')} bytes`;
        get('package-hash').textContent = value.sha256;
        get('release-link').href = `https://github.com/dspaudio/mc-1/releases/tag/v${value.version}`;
        get('package-details').hidden = false;
        get('package-status').textContent = 'SHA256, package CRCs, identity and update loader verified.';
        controls();
        if (!busy && selected().every(Boolean)) await identify();
    } catch (error) {
        get('package-status').textContent = `Release unavailable: ${error.message}. Use the release link or reload to retry.`;
        get('package-status').setAttribute('role', 'alert');
    }
}

controls();
if (!navigator.requestMIDIAccess || !isSecureContext)
    message('Web MIDI SysEx is unavailable here. Open this HTTPS page in a supported desktop browser, or use the desktop installer.', 'error');
void fetchRelease();

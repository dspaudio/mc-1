// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 dspaudio
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {crc16, jlEnc, loadPackage} from '../site/firmware-ota.js';
import {prepareStockRestore, STOCK_V15_SHA256, STOCK_V15_SIZE} from '../site/firmware-stock.js';

const digest = async b => Buffer.from(await crypto.subtle.digest('SHA-256', b)).toString('hex');
const rd32 = (b, i) => new DataView(b.buffer, b.byteOffset).getUint32(i, true);
const w16 = (b, i, n) => new DataView(b.buffer).setUint16(i, n, true);
const w32 = (b, i, n) => new DataView(b.buffer).setUint32(i, n, true);
function wrap(image, product) {
    const raw = new Uint8Array(image.length + 20);
    for (let i = 0; i < 20; i++) {
        raw.set(image.subarray(i * 47, i * 47 + 47), i * 48);
        raw[i * 48 + 47] = i < product.length ? product.charCodeAt(i) + i + 1 : 0x7d;
    }
    raw.set(image.subarray(940), 960);
    return raw;
}
function fixture(stock) {
    const flashOff = 1024, flashSize = 0x93000, otaOff = flashOff + flashSize;
    const otaSize = stock ? 19969 : 600;
    const image = new Uint8Array(stock ? STOCK_V15_SIZE - 20 : otaOff + otaSize);
    image.fill(stock ? 42 : 99, flashOff, otaOff);
    image.fill(stock ? 19 : 37, otaOff, otaOff + otaSize);
    if (!stock) image.set(new TextEncoder().encode('FELUCCA-LOADER-1'), otaOff + 72);
    for (const [i, type, off, size] of [[0, 0, flashOff, flashSize], [1, 100, otaOff, otaSize]]) {
        const e = new Uint8Array(80);
        w16(e, 0, type); w16(e, 4, crc16(image.subarray(off, off + size)));
        w32(e, 8, off); w32(e, 12, size);
        image.set(jlEnc(e), 64 + i * 80);
    }
    const header = new Uint8Array(64);
    w16(header, 8, 2); w32(header, 4, image.length);
    w16(header, 2, crc16(image.subarray(64, 224)));
    w16(header, 0, crc16(header.subarray(2)));
    image.set(jlEnc(header));
    return {raw: wrap(image, stock ? 'FM-1_015' : 'FM-1_900'), image, flashOff, flashSize, otaOff, otaSize};
}
const stock = fixture(true), loader = fixture(false), loaderSha256 = await digest(loader.raw);
const originalCrypto = Object.getOwnPropertyDescriptor(globalThis, 'crypto');
const realCrypto = globalThis.crypto;
// Substitute only the unavailable vendor fixture's pinned digest, not package CRCs
// or any other hash. Real vendor-file integration below uses the actual WebCrypto.
Object.defineProperty(globalThis, 'crypto', {configurable: true, value: {subtle: {
    digest: async (algorithm, bytes) => Buffer.from(bytes).equals(Buffer.from(stock.raw))
        ? Buffer.from(STOCK_V15_SHA256, 'hex')
        : realCrypto.subtle.digest(algorithm, bytes),
}}});
try {
    const prepared = await prepareStockRestore(stock.raw, loader.raw, {loaderSha256});
    const verified = await loadPackage(prepared.raw, {sha256: prepared.sha256});
    assert.equal(verified.product, 'FM-1_015');
    assert.deepEqual(verified.image.subarray(loader.flashOff, loader.otaOff),
        stock.image.subarray(stock.flashOff, stock.otaOff));
    assert.deepEqual(verified.image.subarray(loader.otaOff), loader.image.subarray(loader.otaOff));
    assert.deepEqual(stock.raw, fixture(true).raw);
    assert.deepEqual(loader.raw, fixture(false).raw);
    const fromBuffers = await prepareStockRestore(stock.raw.buffer, loader.raw.buffer, {loaderSha256});
    assert.deepEqual(fromBuffers.raw, prepared.raw);
    const changingStock = stock.raw.slice(), changingLoader = loader.raw.slice();
    const inFlight = prepareStockRestore(changingStock, changingLoader, {loaderSha256});
    changingStock.fill(0); changingLoader.fill(0);
    assert.deepEqual((await inFlight).raw, prepared.raw);
    const damaged = stock.raw.slice(); damaged[1200] ^= 1;
    await assert.rejects(prepareStockRestore(damaged, loader.raw, {loaderSha256}), {code: 'badstock'});
    await assert.rejects(prepareStockRestore(stock.raw.subarray(1), loader.raw, {loaderSha256}), {code: 'badstock'});
    await assert.rejects(prepareStockRestore(stock.raw, loader.raw), {code: 'badstock'});
    await assert.rejects(prepareStockRestore(stock.raw, loader.raw, {loaderSha256: '0'.repeat(64)}), {code: 'badpkg'});
    const foreign = wrap(loader.image, 'FM-1_901');
    await assert.rejects(prepareStockRestore(stock.raw, foreign, {loaderSha256: await digest(foreign)}), {code: 'badstock'});
} finally {
    Object.defineProperty(globalThis, 'crypto', originalCrypto);
}

// Optional private integration inputs, never stored or distributed by the repository.
if (process.argv.length > 2) {
    assert.equal(process.argv.length, 4);
    const [official, release] = await Promise.all(process.argv.slice(2).map(p => readFile(p)));
    const prepared = await prepareStockRestore(official, release, {loaderSha256: await digest(release)});
    const {image, product} = await loadPackage(prepared.raw, {sha256: prepared.sha256});
    assert.equal(product, 'FM-1_015');
    const e = jlEnc(image.slice(64, 144)), off = rd32(e, 8);
    assert.deepEqual(image.subarray(off, off + 0x93000), new Uint8Array(official.subarray(1044, 1044 + 0x93000)));
    const original = await loadPackage(release);
    const ota = jlEnc(image.slice(144, 224)), originalOta = jlEnc(original.image.slice(144, 224));
    assert.deepEqual(image.subarray(rd32(ota, 8), rd32(ota, 8) + rd32(ota, 12)),
        original.image.subarray(rd32(originalOta, 8), rd32(originalOta, 8) + rd32(originalOta, 12)));
    console.log(`STOCK_V15_REAL_PACKAGE_PASS ${prepared.sha256}`);
}
console.log('STOCK_RESTORE_PASS');

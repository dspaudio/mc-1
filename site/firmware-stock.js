// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 dspaudio
// V15 file identity and layout follow Felucca-WebApp's validateStockPackage,
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.
import {crc16, InstallError, jlEnc, loadPackage} from './firmware-ota.js';

export const STOCK_V15_SHA256 = 'db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a';
export const STOCK_V15_SIZE = 699956;
const rd16 = (b, i) => b[i] | b[i + 1] << 8;
const rd32 = (b, i) => (b[i] | b[i + 1] << 8 | b[i + 2] << 16 | b[i + 3] << 24) >>> 0;
const wr16 = (b, i, n) => { b[i] = n & 255; b[i + 1] = n >>> 8; };
const fail = message => { throw new InstallError('badstock', message); };

async function digest(bytes) {
    return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)),
        b => b.toString(16).padStart(2, '0')).join('');
}

function unpack(raw) {
    if (raw.length < 960) fail('Not an FM-1 package');
    const image = new Uint8Array(raw.length - 20);
    let product = '';
    for (let i = 0; i < 20; i++) {
        image.set(raw.subarray(i * 48, i * 48 + 47), i * 47);
        if (raw[i * 48 + 47] !== 0x7d)
            product += String.fromCharCode((raw[i * 48 + 47] - i - 1) & 255);
    }
    image.set(raw.subarray(960), 940);
    const header = jlEnc(image.slice(0, 64));
    const count = rd16(header, 8);
    if (crc16(header.subarray(2)) !== rd16(header, 0) || count < 1 || count > 8 ||
            crc16(image.subarray(64, 64 + count * 80)) !== rd16(header, 2))
        fail('Damaged FM-1 package header');
    const entries = [];
    for (let i = 0; i < count; i++) {
        const position = 64 + i * 80;
        const bytes = jlEnc(image.slice(position, position + 80));
        entries.push({position, bytes, type: rd16(bytes, 0), offset: rd32(bytes, 8), size: rd32(bytes, 12)});
    }
    return {image, product, header, entries};
}

export async function prepareStockRestore(stockBytes, loaderBytes, {loaderSha256} = {}) {
    // Snapshot both inputs before the first await; file changes cannot alter a verified transfer.
    const stockRaw = Uint8Array.from(new Uint8Array(stockBytes));
    const loaderRaw = Uint8Array.from(new Uint8Array(loaderBytes));
    if (stockRaw.length !== STOCK_V15_SIZE || await digest(stockRaw) !== STOCK_V15_SHA256)
        fail('Select the unmodified official FM-1 V15 file (FM-1.fwsc)');
    if (!/^[a-f0-9]{64}$/i.test(loaderSha256 || ''))
        fail('A SHA256-verified MC-1 update loader is required');
    const stock = unpack(stockRaw);
    const stockFlash = stock.entries.filter(e => e.type === 0);
    const stockOta = stock.entries.filter(e => e.type === 100);
    if (stock.product !== 'FM-1_015' || stockFlash.length !== 1 || stockOta.length !== 1 ||
            stockFlash[0].offset !== 1024 || stockFlash[0].size !== 0x93000 || stockOta[0].size !== 19969)
        fail('The official firmware has an unsupported FM-1 layout');
    const verified = await loadPackage(loaderRaw, {sha256: loaderSha256});
    const loader = unpack(loaderRaw);
    const flash = loader.entries.filter(e => e.type === 0);
    const ota = loader.entries.filter(e => e.type === 100);
    if (verified.product !== 'FM-1_900' || loader.entries.length !== 2 || flash.length !== 1 ||
            flash[0].size !== 0x93000 || ota.length !== 1 || ota[0].size < 512 || ota[0].size > 0x4000 ||
            flash[0].offset < 224 || flash[0].offset + flash[0].size > ota[0].offset)
        fail('The MC-1 package has an unsupported update-loader layout');

    // Keep the tested small RAM loader and its separate recovery-record sector.
    // Only the source of flash.bin changes; no boot/config bytes are written by this loader.
    const entry = flash[0];
    const officialFlash = stock.image.subarray(stockFlash[0].offset, stockFlash[0].offset + stockFlash[0].size);
    if (crc16(officialFlash) !== rd16(stockFlash[0].bytes, 4))
        fail('Official flash image CRC mismatch');
    loader.image.set(officialFlash, entry.offset);
    wr16(entry.bytes, 4, crc16(officialFlash));
    loader.image.set(jlEnc(entry.bytes), entry.position);
    wr16(loader.header, 2, crc16(loader.image.subarray(64, 224)));
    wr16(loader.header, 0, crc16(loader.header.subarray(2)));
    loader.image.set(jlEnc(loader.header));
    const raw = new Uint8Array(loader.image.length + 20);
    const product = 'FM-1_015';
    for (let i = 0; i < 20; i++) {
        raw.set(loader.image.subarray(i * 47, i * 47 + 47), i * 48);
        raw[i * 48 + 47] = i < product.length ? product.charCodeAt(i) + i + 1 : 0x7d;
    }
    raw.set(loader.image.subarray(940), 960);
    await loadPackage(raw);
    return {raw, sha256: await digest(raw), product};
}

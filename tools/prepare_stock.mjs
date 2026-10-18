// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 dspaudio
// Prepare locally; never connect to or write an instrument.
import {readFile, writeFile} from 'node:fs/promises';
import {prepareStockRestore} from '../site/firmware-stock.js';

const [stockPath, loaderPath, outputPath] = process.argv.slice(2);
if (!stockPath || !loaderPath || !outputPath || process.argv.length !== 5)
    throw new Error('Usage: node tools/prepare_stock.mjs FM-1.fwsc mc-1-release.fwsc output.fwsc');
const manifest = JSON.parse(await readFile(new URL('../FIRMWARE.json', import.meta.url), 'utf8'));
const [stock, loader] = await Promise.all([readFile(stockPath), readFile(loaderPath)]);
const prepared = await prepareStockRestore(stock, loader, {loaderSha256: manifest.sha256});
await writeFile(outputPath, prepared.raw, {flag: 'wx'});
console.log(JSON.stringify({
    product: prepared.product, bytes: prepared.raw.length, sha256: prepared.sha256, output: outputPath,
    status: 'Prepared only; no device accessed. Official app bytes with the MC-1 RAM update loader.',
}, null, 2));

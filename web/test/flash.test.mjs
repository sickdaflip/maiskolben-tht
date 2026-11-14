// SPDX-License-Identifier: GPL-2.0-or-later
// Exercises the STK500 uploader against an emulated ATmega328P bootloader.
// Run: node web/test/flash.test.mjs [path/to/firmware.hex]
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import { parseHex } from '../src/hex.js';
import { flash } from '../src/stk500.js';

globalThis.performance ??= { now: () => Date.now() };

class FakeBootloader {
	constructor(baud) {
		this.baud = baud;
		this.flash = new Uint8Array(32768).fill(0xff);
		this.addr = 0;
		this.rx = [];
		this.started = false;
	}
	reply(bytes) { this.out(Uint8Array.from(bytes)); }
	process() {
		const need = { 0x30: 2, 0x75: 2, 0x50: 2, 0x51: 2, 0x55: 4, 0x42: 22 };
		for (;;) {
			if (!this.rx.length) return;
			const cmd = this.rx[0];
			let len = need[cmd];
			if (cmd === 0x64 || cmd === 0x74) {
				if (this.rx.length < 4) return;
				const size = (this.rx[1] << 8) | this.rx[2];
				len = cmd === 0x64 ? 5 + size : 5;
			}
			if (len === undefined) { this.rx.shift(); continue; }
			if (this.rx.length < len) return;
			const pkt = this.rx.splice(0, len);
			if (pkt[len - 1] !== 0x20) continue;
			switch (cmd) {
				case 0x75: this.reply([0x14, 0x1e, 0x95, 0x0f, 0x10]); break;
				case 0x55: this.addr = (pkt[1] | (pkt[2] << 8)) * 2; this.reply([0x14, 0x10]); break;
				case 0x64: {
					const size = (pkt[1] << 8) | pkt[2];
					this.flash.set(pkt.slice(4, 4 + size), this.addr);
					this.reply([0x14, 0x10]);
					break;
				}
				case 0x74: {
					const size = (pkt[1] << 8) | pkt[2];
					this.reply([0x14, ...this.flash.slice(this.addr, this.addr + size), 0x10]);
					break;
				}
				case 0x51: this.started = true; this.reply([0x14, 0x10]); break;
				default: this.reply([0x14, 0x10]);
			}
		}
	}
}

class FakePort {
	constructor(boot) { this.boot = boot; this.baud = 0; }
	async open({ baudRate }) {
		this.baud = baudRate;
		let ctrl;
		this.readable = new ReadableStream({ start: (c) => { ctrl = c; } });
		this.ctrl = ctrl;
		this.boot.out = (b) => { try { ctrl.enqueue(b); } catch { /* closed */ } };
		this.writable = new WritableStream({
			write: (chunk) => {
				// Wrong baud rate: the bootloader sees garbage and stays silent.
				if (this.baud !== this.boot.baud) return;
				this.boot.rx.push(...chunk);
				setTimeout(() => this.boot.process(), 1);
			},
		});
	}
	async setSignals() { this.boot.rx = []; }
	async close() { try { this.ctrl.close(); } catch { /* already closed */ } }
}

const hexPath = process.argv[2] ?? new URL('../../.pio/build/nano/firmware.hex', import.meta.url);
const image = parseHex(readFileSync(hexPath, 'utf8'));
console.log(`image: ${image.length} bytes`);

for (const [label, baud, mode] of [['old bootloader, auto', 57600, 'auto'], ['optiboot, auto', 115200, 'auto'], ['optiboot, explicit', 115200, 'optiboot']]) {
	const boot = new FakeBootloader(baud);
	const t0 = Date.now();
	const stages = new Set();
	await flash(new FakePort(boot), image, { bootloader: mode, onProgress: (s) => stages.add(s), log: () => {} });
	assert.deepEqual(boot.flash.slice(0, image.length), image, 'flash content');
	assert.ok(boot.started, 'application started');
	assert.ok(stages.has('verify') && stages.has('done'), 'stages');
	console.log(`ok  ${label} (${Date.now() - t0} ms)`);
}

// A wrong explicit bootloader must fail cleanly.
await assert.rejects(flash(new FakePort(new FakeBootloader(57600)), image, { bootloader: 'optiboot' }), /no response/);
console.log('ok  wrong bootloader rejected');

// Corrupted hex must be rejected by the parser.
assert.throws(() => parseHex(':10000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF01\n'), /checksum/);
console.log('ok  checksum error detected');

// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * STK500v1 uploader for ATmega328P boards with Optiboot (115200 baud)
 * or the old ATmegaBOOT bootloader found on most Nano clones (57600 baud).
 */
import { SerialLink, sleep } from './serial.js';

const STK_OK = 0x10;
const STK_INSYNC = 0x14;
const CRC_EOP = 0x20;

const PAGE_SIZE = 128;
const FLASH_LIMIT = 30720;  // 32 KB minus the largest (2 KB) bootloader
const SIGNATURES = ['1e950f', '1e9514'];  // ATmega328P, ATmega328

export const BOOTLOADERS = {
	optiboot: { baud: 115200 },
	old: { baud: 57600 },
};

class Programmer {
	constructor(link) {
		this.link = link;
	}

	async command(bytes, replyLen = 0, timeout = 400) {
		await this.link.write([...bytes, CRC_EOP]);
		const head = await this.link.read(1, timeout);
		if (head[0] !== STK_INSYNC) throw new Error(`not in sync (0x${head[0].toString(16)})`);
		const data = replyLen ? await this.link.read(replyLen, timeout) : new Uint8Array();
		const tail = await this.link.read(1, timeout);
		if (tail[0] !== STK_OK) throw new Error('command failed');
		return data;
	}

	async sync(attempts = 10) {
		for (let i = 0; i < attempts; i++) {
			this.link.drain();
			try {
				await this.link.write([0x30, CRC_EOP]);
				const r = await this.link.read(2, 150);
				if (r[0] === STK_INSYNC && r[1] === STK_OK) return true;
			} catch { /* retry */ }
			await sleep(20);
		}
		return false;
	}

	async signature() {
		const s = await this.command([0x75], 3);
		return Array.from(s, (b) => b.toString(16).padStart(2, '0')).join('');
	}

	async setDevice() {
		// Device parameters for the ATmega328P; bootloaders accept and ignore them.
		await this.command([0x42, 0x86, 0, 0, 1, 1, 1, 1, 3, 0xff, 0xff, 0xff, 0xff,
			0, PAGE_SIZE, 0x04, 0x00, 0, 0, 0x80, 0x00]);
	}

	async loadAddress(byteAddr) {
		const word = byteAddr >> 1;
		await this.command([0x55, word & 0xff, (word >> 8) & 0xff]);
	}

	async writePage(data) {
		await this.command([0x64, 0x00, data.length, 0x46, ...data], 0, 1000);
	}

	async readPage(len) {
		return this.command([0x74, 0x00, len, 0x46], len, 1000);
	}
}

/**
 * Flashes `image` (Uint8Array, application only) to the board on `port`.
 * `bootloader` is "auto", "optiboot" or "old". `onProgress(stage, fraction)`
 * reports "connect", "write", "verify" and "done"; `log(text)` gets details.
 */
export async function flash(port, image, { bootloader = 'auto', verify = true, onProgress = () => {}, log = () => {} } = {}) {
	if (image.length > FLASH_LIMIT) {
		throw new Error(`firmware too large (${image.length} bytes, max ${FLASH_LIMIT})`);
	}
	const order = bootloader === 'auto' ? ['old', 'optiboot'] : [bootloader];
	let link = null;
	let prog = null;
	onProgress('connect', 0);

	for (const name of order) {
		const baud = BOOTLOADERS[name].baud;
		log(`Trying ${name} bootloader at ${baud} baud`);
		link = new SerialLink(port);
		await link.open(baud);
		await link.resetBoard();
		prog = new Programmer(link);
		if (await prog.sync(12)) {
			log(`In sync at ${baud} baud`);
			break;
		}
		await link.close();
		link = null;
	}
	if (!link) throw new Error('no response from the bootloader');

	try {
		const sig = await prog.signature();
		log(`Device signature ${sig}`);
		if (!SIGNATURES.includes(sig)) throw new Error(`unexpected device signature ${sig}`);
		await prog.setDevice();
		await prog.command([0x50]);  // enter programming mode

		const pages = Math.ceil(image.length / PAGE_SIZE);
		const padded = new Uint8Array(pages * PAGE_SIZE).fill(0xff);
		padded.set(image);

		for (let p = 0; p < pages; p++) {
			const addr = p * PAGE_SIZE;
			await prog.loadAddress(addr);
			await prog.writePage(padded.subarray(addr, addr + PAGE_SIZE));
			onProgress('write', (p + 1) / pages);
		}
		log(`Wrote ${image.length} bytes in ${pages} pages`);

		if (verify) {
			for (let p = 0; p < pages; p++) {
				const addr = p * PAGE_SIZE;
				await prog.loadAddress(addr);
				const data = await prog.readPage(PAGE_SIZE);
				for (let i = 0; i < PAGE_SIZE; i++) {
					if (data[i] !== padded[addr + i]) {
						throw new Error(`verify failed at 0x${(addr + i).toString(16)}`);
					}
				}
				onProgress('verify', (p + 1) / pages);
			}
			log('Verify OK');
		}
		await prog.command([0x51]);  // leave programming mode, start the app
		onProgress('done', 1);
	} finally {
		await link.close();
	}
}

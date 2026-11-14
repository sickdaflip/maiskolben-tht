// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Intel HEX parser producing a flat flash image.
 * Unused bytes are filled with 0xFF (erased flash).
 */
export function parseHex(text) {
	const chunks = [];
	let base = 0;
	let max = 0;
	const lines = text.split(/\r?\n/);
	for (let n = 0; n < lines.length; n++) {
		const line = lines[n].trim();
		if (!line) continue;
		if (line[0] !== ':') throw new Error(`Line ${n + 1}: missing start code`);
		const bytes = [];
		for (let i = 1; i < line.length; i += 2) bytes.push(parseInt(line.substr(i, 2), 16));
		if (bytes.some(Number.isNaN)) throw new Error(`Line ${n + 1}: invalid hex digits`);
		const sum = bytes.reduce((a, b) => (a + b) & 0xff, 0);
		if (sum !== 0) throw new Error(`Line ${n + 1}: checksum mismatch`);
		const len = bytes[0];
		const addr = (bytes[1] << 8) | bytes[2];
		const type = bytes[3];
		const data = bytes.slice(4, 4 + len);
		if (type === 0x00) {
			const start = base + addr;
			chunks.push([start, data]);
			max = Math.max(max, start + len);
		} else if (type === 0x01) {
			break;
		} else if (type === 0x02) {
			base = ((data[0] << 8) | data[1]) << 4;
		} else if (type === 0x04) {
			base = ((data[0] << 8) | data[1]) << 16;
		}
	}
	const image = new Uint8Array(max).fill(0xff);
	for (const [start, data] of chunks) image.set(data, start);
	return image;
}

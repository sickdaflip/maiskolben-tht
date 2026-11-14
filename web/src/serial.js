// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Thin wrapper around a Web Serial port with a byte buffer, timed reads
 * and line events for the telemetry monitor.
 */
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export class SerialLink {
	constructor(port) {
		this.port = port;
		this.buffer = [];
		this.waiters = [];
		this.reader = null;
		this.writer = null;
		this.onLine = null;
		this.lineBuf = '';
		this.decoder = new TextDecoder();
		this.loop = null;
	}

	async open(baudRate) {
		await this.port.open({ baudRate, bufferSize: 4096 });
		this.writer = this.port.writable.getWriter();
		this.reader = this.port.readable.getReader();
		this.loop = this.readLoop();
	}

	async readLoop() {
		try {
			for (;;) {
				const { value, done } = await this.reader.read();
				if (done) break;
				if (!value) continue;
				for (const b of value) this.buffer.push(b);
				if (this.onLine) this.feedLines(value);
				this.wake();
			}
		} catch {
			// Port closed or device unplugged; close() handles cleanup.
		}
	}

	feedLines(bytes) {
		this.lineBuf += this.decoder.decode(bytes, { stream: true });
		let idx;
		while ((idx = this.lineBuf.indexOf('\n')) >= 0) {
			const line = this.lineBuf.slice(0, idx).replace(/\r$/, '');
			this.lineBuf = this.lineBuf.slice(idx + 1);
			this.onLine(line);
		}
		if (this.lineBuf.length > 4096) this.lineBuf = '';
		this.buffer.length = 0;  // line mode does not use the byte buffer
	}

	wake() {
		const waiters = this.waiters;
		this.waiters = [];
		waiters.forEach((w) => w());
	}

	/** Reads exactly n bytes or throws after `timeout` ms. */
	async read(n, timeout = 500) {
		const deadline = performance.now() + timeout;
		while (this.buffer.length < n) {
			const left = deadline - performance.now();
			if (left <= 0) throw new Error('timeout');
			await Promise.race([
				new Promise((r) => this.waiters.push(r)),
				sleep(Math.min(left, 50)),
			]);
		}
		return Uint8Array.from(this.buffer.splice(0, n));
	}

	drain() {
		this.buffer.length = 0;
	}

	async write(bytes) {
		await this.writer.write(bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes));
	}

	async writeText(text) {
		await this.write(new TextEncoder().encode(text));
	}

	/** Pulses DTR/RTS to trigger the Arduino auto-reset circuit. */
	async resetBoard() {
		await this.port.setSignals({ dataTerminalReady: false, requestToSend: false });
		await sleep(120);
		await this.port.setSignals({ dataTerminalReady: true, requestToSend: true });
		await sleep(30);
	}

	async close() {
		try {
			if (this.reader) {
				await this.reader.cancel();
				this.reader.releaseLock();
			}
		} catch { /* already released */ }
		try {
			if (this.writer) this.writer.releaseLock();
		} catch { /* already released */ }
		try {
			await this.loop;
		} catch { /* ignore */ }
		try {
			await this.port.close();
		} catch { /* already closed */ }
		this.reader = this.writer = null;
	}
}

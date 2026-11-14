// SPDX-License-Identifier: GPL-2.0-or-later
import Alpine from './vendor/alpine.esm.js';
import { drawChart } from './chart.js';
import { parseHex } from './hex.js';
import { LANGUAGES, detectLanguage, translate } from './i18n.js';
import { SerialLink } from './serial.js';
import { flash } from './stk500.js';

const HISTORY = 600;  // 60 s of telemetry at 10 Hz
const SETTLE_SAMPLES = 48;
const COLORS = { temp: '#2563eb', target: '#9ca3af', output: '#f59e0b' };

// Chart data lives outside Alpine's reactive proxies; it changes 10x per second.
const hist = { temp: [], target: [], output: [] };
let settleData = [];

Alpine.data('flasher', () => ({
	lang: detectLanguage(),
	languages: Object.entries(LANGUAGES).map(([code, s]) => ({ code, name: s.langName })),
	supported: 'serial' in navigator,

	manifest: null,
	source: 'release',
	bootloader: 'auto',
	verify: true,
	file: null,
	busy: false,
	stage: '',
	progress: 0,
	result: '',
	resultOk: false,
	logLines: [],

	monitor: null,
	live: null,
	setInput: 330,
	settleCapture: false,
	settleInfo: '',
	settleError: false,
	redrawQueued: false,

	t(key, vars) {
		return translate(this.lang, key, vars);
	},

	async init() {
		document.documentElement.lang = this.lang;
		this.$watch('lang', (l) => {
			document.documentElement.lang = l;
			try { localStorage.setItem('mk-lang', l); } catch { /* storage unavailable */ }
		});
		try {
			const r = await fetch('firmware/manifest.json', { cache: 'no-store' });
			if (r.ok) this.manifest = await r.json();
		} catch { /* no bundled firmware, local file only */ }
		if (!this.manifest) this.source = 'file';
		window.addEventListener('resize', () => this.queueRedraw());
		matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => this.queueRedraw());
	},

	// ------------------------------------------------------------------ flash
	get stageText() {
		return {
			connect: this.t('stageConnect'),
			write: this.t('stageWrite'),
			verify: this.t('stageVerify'),
			done: this.t('stageDone'),
		}[this.stage] ?? '';
	},

	pickFile(event) {
		this.file = event.target.files[0] ?? null;
	},

	async loadImage() {
		if (this.source === 'release' && this.manifest) {
			const r = await fetch(`firmware/${this.manifest.hex}`, { cache: 'no-store' });
			if (!r.ok) throw new Error(`HTTP ${r.status}`);
			return parseHex(await r.text());
		}
		if (!this.file) throw new Error(this.t('noFile'));
		return parseHex(await this.file.text());
	},

	async startFlash() {
		this.result = '';
		this.logLines = [];
		let image;
		try {
			image = await this.loadImage();
		} catch (e) {
			this.fail(e);
			return;
		}
		let port;
		try {
			port = await navigator.serial.requestPort();
		} catch {
			return;  // user closed the port picker
		}
		if (this.monitor) await this.stopMonitor();
		this.busy = true;
		this.stage = 'connect';
		this.progress = 0;
		try {
			await flash(port, image, {
				bootloader: this.bootloader,
				verify: this.verify,
				log: (line) => this.logLines.push(line),
				onProgress: (stage, f) => {
					this.stage = stage;
					if (stage === 'write') this.progress = this.verify ? f / 2 : f;
					else if (stage === 'verify') this.progress = 0.5 + f / 2;
					else if (stage === 'done') this.progress = 1;
				},
			});
			this.result = this.t('stageDone');
			this.resultOk = true;
		} catch (e) {
			this.fail(e);
		} finally {
			this.busy = false;
		}
	},

	fail(e) {
		const msg = String(e?.message ?? e);
		this.result = msg.includes('no response') ? this.t('noBootloader') : this.t('flashError', { message: msg });
		this.resultOk = false;
		this.stage = '';
		this.logLines.push(msg);
	},

	// ---------------------------------------------------------------- monitor
	async toggleMonitor() {
		if (this.monitor) {
			await this.stopMonitor();
			return;
		}
		let port;
		try {
			port = await navigator.serial.requestPort();
		} catch {
			return;
		}
		const link = new SerialLink(port);
		link.onLine = (line) => this.onLine(line);
		try {
			await link.open(115200);
		} catch (e) {
			this.result = this.t('flashError', { message: e.message });
			this.resultOk = false;
			return;
		}
		hist.temp = [];
		hist.target = [];
		hist.output = [];
		this.live = null;
		this.monitor = link;
		this.queueRedraw();
	},

	async stopMonitor() {
		const link = this.monitor;
		this.monitor = null;
		this.live = null;
		await Alpine.raw(link)?.close();
	},

	onLine(line) {
		if (line.startsWith('#')) {
			if (line.includes('refused') || line.includes('switch the iron off')) {
				this.settleCapture = false;
				this.settleError = true;
				this.settleInfo = this.t('settleRefused');
			} else if (line.includes('us;adc')) {
				settleData = [];
				this.settleCapture = true;
			}
			return;
		}
		const f = line.split(';');
		if (f.length === 2 && this.settleCapture) {
			settleData.push([Number(f[0]), Number(f[1])]);
			if (settleData.length >= SETTLE_SAMPLES) this.finishSettle();
			return;
		}
		if (f.length !== 13) return;
		const n = f.map(Number);
		const on = n[3] === 1;
		const standby = n[5] === 1;
		const tip = n[8] !== 999;
		this.live = {
			presets: n.slice(0, 3),
			on,
			error: n[4],
			standby,
			set: n[7],
			temp: tip ? n[8] : null,
			output: Math.round(n[9] * 100),
		};
		push(hist.temp, tip ? n[8] : NaN);
		push(hist.target, on ? (standby ? 150 : n[7]) : NaN);
		push(hist.output, on ? n[9] * 100 : 0);
		this.queueRedraw();
	},

	finishSettle() {
		this.settleCapture = false;
		this.settleError = false;
		const values = settleData.map((p) => p[1]);
		const tail = values.slice(-8);
		const final = tail.reduce((a, b) => a + b, 0) / tail.length;
		let idx = values.length - 1;
		while (idx > 0 && Math.abs(values[idx - 1] - final) <= 3) idx--;
		this.settleInfo = this.t('settleResult', { us: settleData[idx][0] });
		this.queueRedraw();
	},

	get stateText() {
		const l = this.live;
		if (!l) return this.t('waiting');
		if (l.error) return this.t(`err${l.error}`);
		if (!l.on) return this.t('stateOff');
		return l.standby ? this.t('stateStandby') : this.t('stateOn');
	},

	async send(cmd) {
		if (this.monitor) await Alpine.raw(this.monitor).writeText(cmd);
	},

	async applySetpoint() {
		const v = Math.round(Number(this.setInput));
		if (!(v >= 200 && v <= 450)) return;
		await this.send(`T${String(v).padStart(3, '0')}`);
	},

	async recordSettle() {
		this.settleInfo = '';
		this.settleError = false;
		settleData = [];
		this.queueRedraw();
		await this.send('D');
	},

	queueRedraw() {
		if (this.redrawQueued) return;
		this.redrawQueued = true;
		requestAnimationFrame(() => {
			this.redrawQueued = false;
			if (this.$refs.chart) {
				drawChart(this.$refs.chart, {
					series: [
						{ data: hist.target, color: COLORS.target, width: 1.5 },
						{ data: hist.temp, color: COLORS.temp },
					],
					right: { data: hist.output, color: COLORS.output },
				});
			}
			if (this.$refs.settle) {
				drawChart(this.$refs.settle, {
					series: [{ data: settleData.map((p) => p[1]), color: COLORS.temp }],
					xLabel: '0 – 12 ms',
					minSpan: 10,
				});
			}
		});
	},
}));

function push(arr, v) {
	arr.push(v);
	if (arr.length > HISTORY) arr.shift();
}

Alpine.start();

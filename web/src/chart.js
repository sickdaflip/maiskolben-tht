// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Minimal canvas line chart: one or more series on a left axis and an
 * optional series on a right axis (0..100 %).
 */
export function drawChart(canvas, { series, right = null, xLabel = '', minSpan = 20 }) {
	const dpr = window.devicePixelRatio || 1;
	const w = canvas.clientWidth;
	const h = canvas.clientHeight;
	if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
		canvas.width = w * dpr;
		canvas.height = h * dpr;
	}
	const ctx = canvas.getContext('2d');
	ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
	ctx.clearRect(0, 0, w, h);

	const dark = matchMedia('(prefers-color-scheme: dark)').matches;
	const grid = dark ? '#262626' : '#e5e7eb';
	const label = dark ? '#a3a3a3' : '#6b7280';
	const pad = { l: 40, r: right ? 40 : 10, t: 10, b: 22 };
	const pw = w - pad.l - pad.r;
	const ph = h - pad.t - pad.b;

	const all = series.flatMap((s) => s.data.filter((v) => Number.isFinite(v)));
	let lo = all.length ? Math.min(...all) : 0;
	let hi = all.length ? Math.max(...all) : 100;
	if (hi - lo < minSpan) {
		const mid = (hi + lo) / 2;
		lo = mid - minSpan / 2;
		hi = mid + minSpan / 2;
	}
	const step = niceStep((hi - lo) / 4);
	lo = Math.floor(lo / step) * step;
	hi = Math.ceil(hi / step) * step;

	ctx.font = '11px ui-sans-serif, system-ui, sans-serif';
	ctx.lineWidth = 1;
	ctx.textBaseline = 'middle';
	for (let v = lo; v <= hi + 1e-9; v += step) {
		const y = pad.t + ph - ((v - lo) / (hi - lo)) * ph;
		ctx.strokeStyle = grid;
		ctx.beginPath();
		ctx.moveTo(pad.l, y);
		ctx.lineTo(pad.l + pw, y);
		ctx.stroke();
		ctx.fillStyle = label;
		ctx.textAlign = 'right';
		ctx.fillText(Math.round(v), pad.l - 6, y);
	}
	if (right) {
		ctx.textAlign = 'left';
		for (let v = 0; v <= 100; v += 25) {
			const y = pad.t + ph - (v / 100) * ph;
			ctx.fillStyle = label;
			ctx.fillText(`${v}%`, pad.l + pw + 6, y);
		}
	}
	if (xLabel) {
		ctx.textAlign = 'center';
		ctx.fillStyle = label;
		ctx.fillText(xLabel, pad.l + pw / 2, h - 8);
	}

	const plot = (data, color, scale, width = 2) => {
		if (data.length < 2) return;
		ctx.strokeStyle = color;
		ctx.lineWidth = width;
		ctx.lineJoin = 'round';
		ctx.beginPath();
		let started = false;
		data.forEach((v, i) => {
			if (!Number.isFinite(v)) {
				started = false;
				return;
			}
			const x = pad.l + (i / (data.length - 1)) * pw;
			const y = pad.t + ph - scale(v) * ph;
			if (started) ctx.lineTo(x, y);
			else ctx.moveTo(x, y);
			started = true;
		});
		ctx.stroke();
	};
	if (right) plot(right.data, right.color, (v) => Math.min(1, Math.max(0, v / 100)), 1.5);
	for (const s of series) plot(s.data, s.color, (v) => (v - lo) / (hi - lo), s.width ?? 2);
}

function niceStep(raw) {
	const exp = Math.pow(10, Math.floor(Math.log10(Math.max(raw, 1e-9))));
	const f = raw / exp;
	return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * exp;
}

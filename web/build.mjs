// SPDX-License-Identifier: GPL-2.0-or-later
// Builds the static flasher site into dist/.
// Bundled firmware (optional) is taken from public/firmware/.
import { execFileSync } from 'node:child_process';
import { cpSync, existsSync, mkdirSync, rmSync } from 'node:fs';

const out = 'dist';
rmSync(out, { recursive: true, force: true });
mkdirSync(`${out}/src/vendor`, { recursive: true });

cpSync('index.html', `${out}/index.html`);
for (const f of ['app.js', 'chart.js', 'hex.js', 'i18n.js', 'serial.js', 'stk500.js']) {
	cpSync(`src/${f}`, `${out}/src/${f}`);
}
cpSync('node_modules/alpinejs/dist/module.esm.min.js', `${out}/src/vendor/alpine.esm.js`);
if (existsSync('public')) cpSync('public', out, { recursive: true });

execFileSync('npx', ['@tailwindcss/cli', '-i', 'src/app.css', '-o', `${out}/app.css`, '--minify'], { stdio: 'inherit' });
console.log(`Site built in ${out}/`);

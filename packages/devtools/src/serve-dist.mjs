#!/usr/bin/env node
/** Serves the ALREADY-BUILT dist/ output (run `npm run build` first) -
 * distinct from serve-dev.mjs, which serves packages/ directly with no
 * build step. Confirms dist/ is actually usable on its own, standing in
 * for "installed/bundled execution works outside the checkout." */
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { startStaticServer } from './static-server.mjs';

const repoRoot = join(fileURLToPath(import.meta.url), '..', '..', '..', '..');
const distDir = join(repoRoot, 'dist');
const port = Number(process.env.PORT ?? 8422);

if (!existsSync(distDir)) {
  console.error(`dist/ not found at ${distDir} - run \`npm run build\` first.`);
  process.exit(1);
}

await startStaticServer(distDir, port);
console.log(`usdk dist server: http://127.0.0.1:${port}/packages/uagent/public/index.html`);
console.log(`Connected-mode demo: http://127.0.0.1:${port}/packages/uagent/public/connected.html`);
console.log('Serving the built dist/ output - if you just edited source, re-run `npm run build` first.');

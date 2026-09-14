#!/usr/bin/env node
import { startStaticServer } from './static-server.mjs';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';

const repoRoot = join(fileURLToPath(import.meta.url), '..', '..', '..', '..');
const port = Number(process.env.PORT ?? 8420);

await startStaticServer(repoRoot, port);
console.log(`usdk dev server: http://127.0.0.1:${port}/packages/uagent/public/index.html`);
console.log('Serving the repo root directly (no build step) - ES modules resolve exactly as written in packages/*/src/.');

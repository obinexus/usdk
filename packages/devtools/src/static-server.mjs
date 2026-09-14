import { createServer } from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { join, extname, resolve } from 'node:path';

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.wasm': 'application/wasm',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
};

/**
 * A zero-dependency static file server, built only on Node's `http`
 * module - no bundler, no external npm package (see the workspace root
 * package.json 'Why zero dependencies'). Serves `root` with directory
 * traversal blocked (every resolved path is checked to still be inside
 * `root`).
 * @param {string} root @param {number} port
 * @returns {Promise<import('node:http').Server>}
 */
export function startStaticServer(root, port) {
  const rootResolved = resolve(root);
  const server = createServer(async (req, res) => {
    try {
      const url = new URL(req.url, 'http://localhost');
      let path = decodeURIComponent(url.pathname);
      if (path.endsWith('/')) path += 'index.html';
      const filePath = resolve(join(rootResolved, path));
      if (!filePath.startsWith(rootResolved)) {
        res.writeHead(403).end('forbidden');
        return;
      }
      const st = await stat(filePath).catch(() => null);
      if (!st || !st.isFile()) {
        res.writeHead(404).end('not found');
        return;
      }
      const body = await readFile(filePath);
      const type = MIME[extname(filePath)] ?? 'application/octet-stream';
      res.writeHead(200, { 'Content-Type': type, 'Cache-Control': 'no-store' }).end(body);
    } catch (e) {
      res.writeHead(500).end(`internal error: ${e && e.message}`);
    }
  });
  return new Promise((resolvePromise) => {
    server.listen(port, '127.0.0.1', () => resolvePromise(server));
  });
}

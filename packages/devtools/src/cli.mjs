#!/usr/bin/env node
/** Single entry point for the three devtools commands - also usable
 * directly as `npx usdk-devtools <command>` per package.json's "bin".
 * The root package.json's own build/serve scripts call the underlying
 * modules directly; this file exists so all three are reachable through
 * one name too. */
const command = process.argv[2];

function usage() {
  console.log('usage: usdk-devtools <command>');
  console.log('  dev          serve packages/ directly, no build step (default port 8420)');
  console.log('  build        build dist/ from the browser-relevant packages');
  console.log('  serve-dist   serve the already-built dist/ output (default port 8422)');
}

switch (command) {
  case 'dev':
    await import('./serve-dev.mjs');
    break;
  case 'build': {
    const { buildDist } = await import('./build-dist.mjs');
    const result = await buildDist();
    console.log(`Built dist/ with ${result.packageDirs.length} packages: ${result.packageDirs.join(', ')}`);
    break;
  }
  case 'serve-dist':
    await import('./serve-dist.mjs');
    break;
  default:
    usage();
    process.exit(command ? 2 : 0);
}

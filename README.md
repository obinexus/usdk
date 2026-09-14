# USDK

USDK is a modular C11 SDK, with a versioned dynamic C ABI, for building
agentic language-model applications around a trilateral agreement
protocol: three independently-built parties - `usdk-perceive`,
`usdk-deliberate`, `usdk-verify` - each cast a distinct, real check
against the same candidate action, and only unanimous acceptance commits
it.

This is a prototype. See [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md)
for exactly what is implemented and tested, and
[`docs/RESEARCH_REVIEW.md`](docs/RESEARCH_REVIEW.md) for the research this
design was checked against (and where it departs from that research, and
why).

## Documentation

- [`docs/RESEARCH_REVIEW.md`](docs/RESEARCH_REVIEW.md) - review of the supplied research archives.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) - the three roles, and comparison with the research.
- [`docs/PACKAGES.md`](docs/PACKAGES.md) - every package, its responsibilities, and the dependency graph.
- [`docs/CONSENSUS_PROTOCOL.md`](docs/CONSENSUS_PROTOCOL.md) - the trilateral agreement protocol, precisely.
- [`docs/ABI.md`](docs/ABI.md) - the dynamic C ABI.
- [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) - build, run the demo, add a driver/binding.
- [`docs/VALIDATION.md`](docs/VALIDATION.md) - what was actually tested, and how to reproduce it.
- [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md) - requirement-by-requirement status.

## Quick start

```bash
make
make test
./build/bin/usdk doctor --json
./build/bin/usdk demo --scenario trilateral-consensus --json
```

See `docs/GETTING_STARTED.md` for prerequisites and Windows/MSYS2 UCRT64 notes.

## License

MIT - see [`LICENSE`](LICENSE). No supplied research archive included a
license file; this is an independent decision for this project (see
`docs/RESEARCH_REVIEW.md`).

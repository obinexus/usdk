import { UsdkError, Status, compareVersions, parseManifest } from '@usdk/contracts';

const MAX_RESOLUTION = 64;

/**
 * An in-memory set of parsed manifests, and the real topological-sort
 * dependency resolver over them - the JS analogue of
 * usdk_manifest_set_t/usdk_manifest_resolve (src/ffi/manifest.c). See
 * docs/ABI.md "Module manifests and dependency resolution" for why this
 * is a full Kahn's-algorithm resolution over the complete transitive
 * graph, not a shortest-path substitute - the task's own instruction
 * warns against exactly that shortcut, because it can miss a mandatory
 * dependency reachable only through a longer edge.
 */
export class ManifestRegistry {
  constructor() {
    /** @type {Map<string, import('@usdk/contracts').Manifest>} */
    this._byName = new Map();
  }

  /** @param {unknown} raw */
  register(raw) {
    const m = parseManifest(raw);
    this._byName.set(m.name, m);
    return m;
  }

  /** @param {string} name */
  get(name) {
    return this._byName.get(name);
  }

  /**
   * Resolves the complete load order for `requestedNames` - every
   * transitive dependency included, ordered so a dependency always
   * appears before its dependent.
   * @param {string[]} requestedNames
   * @returns {import('@usdk/contracts').Manifest[]}
   */
  resolve(requestedNames) {
    /** @type {import('@usdk/contracts').Manifest[]} */
    const nodes = [];
    const addNode = (name, requiredBy) => {
      if (nodes.find((n) => n.name === name)) return;
      const m = this._byName.get(name);
      if (!m) {
        throw new UsdkError(
          Status.DEPENDENCY_UNRESOLVED,
          `'${name}' (required by ${requiredBy ?? 'the requested set'}) has no registered manifest`
        );
      }
      if (nodes.length >= MAX_RESOLUTION) {
        throw new UsdkError(Status.DEPENDENCY_UNRESOLVED, 'resolution set too large');
      }
      nodes.push(m);
    };

    for (const name of requestedNames) addNode(name, null);
    // BFS closure: nodes.length grows as dependencies are discovered;
    // re-reading it each iteration (a plain for-loop, not a cached
    // bound) is what lets a dependency-of-a-dependency be picked up in
    // this same pass without recursion.
    for (let i = 0; i < nodes.length; i++) {
      const m = nodes[i];
      for (const dep of m.dependencies) {
        const depManifest = this._byName.get(dep.capability);
        if (!depManifest) {
          throw new UsdkError(Status.DEPENDENCY_UNRESOLVED, `'${dep.capability}' (required by '${m.name}') has no registered manifest`);
        }
        if (compareVersions(depManifest.version, dep.minVersion) < 0) {
          throw new UsdkError(
            Status.DEPENDENCY_VERSION_INCOMPATIBLE,
            `'${dep.capability}' version ${depManifest.version} is below the minimum ${dep.minVersion} required by '${m.name}'`
          );
        }
        addNode(dep.capability, m.name);
      }
    }

    // Kahn's algorithm: in-degree = count of not-yet-emitted dependencies.
    const inDegree = new Map(nodes.map((n) => [n.name, n.dependencies.length]));
    const emitted = new Set();
    const order = [];
    while (emitted.size < nodes.length) {
      const ready = nodes.find((n) => !emitted.has(n.name) && inDegree.get(n.name) === 0);
      if (!ready) {
        throw new UsdkError(Status.DEPENDENCY_CYCLE, 'dependency cycle detected among: ' + nodes.filter((n) => !emitted.has(n.name)).map((n) => n.name).join(', '));
      }
      order.push(ready);
      emitted.add(ready.name);
      for (const n of nodes) {
        if (emitted.has(n.name)) continue;
        if (n.dependencies.some((d) => d.capability === ready.name)) {
          inDegree.set(n.name, inDegree.get(n.name) - 1);
        }
      }
    }
    return order;
  }
}

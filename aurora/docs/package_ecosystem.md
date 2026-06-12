# Aurora Package Ecosystem

Complete reference for the Aurora package ecosystem, including registries, version resolution, sandboxing, and the voss tool.

---

## 1. Overview

The Aurora package ecosystem consists of:

- **voss** — the package manager CLI (`aurora/tools/voss/`)
- **Registries** — package repositories (GitHub-based or HTTP)
- **Packages** — `.aura` source packages with `aurora.pkg` manifest
- **Bridges** — ecosystem packages (PyPI, npm, Cargo)
- **Lockfile** — `aura.lock` for deterministic reproducible builds

---

## 2. Package Format

### Manifest (`aurora.pkg`)

```
name: my-package
version: 1.0.0
author: Alice
description: A useful Aurora package
entry: main.aura
dependencies:
  - lodash@^4.17
  - serde@>=1.0
permissions:
  - network
  - filesystem
```

Fields:

| Field | Required | Description |
|-------|----------|-------------|
| `name` | Yes | Package name (lowercase, hyphens allowed) |
| `version` | Yes | Semantic version |
| `description` | No | Short description |
| `author` | No | Package author |
| `entry` | Yes | Entry point file (e.g., `main.aura`) |
| `dependencies` | No | Comma-separated list of `name@version` |
| `permissions` | No | Required permissions (`network`, `filesystem`, `process`, etc.) |

### Package Directory Structure

```
packages/my-package/
    aurora.pkg          # manifest
    main.aura           # entry point
    *.aura              # additional source files
    my-package@1.0.0.sig  # package signature (optional)
```

---

## 3. Registries

### Registry Configuration

Registries are stored in `aura.registry`:

```
my-registry https://registry.example.com/api/v1
github-releases github:myuser/packages
```

### Supported Registry Types

| Type | Spec Format | Description |
|------|-------------|-------------|
| GitHub | `github:user/repo` | GitHub Releases as registry |
| HTTP | URL | Generic HTTP registry API |

### Registry Commands

```bash
voss registry add <name> <url>        # Add a registry
voss registry remove <name>           # Remove a registry
voss registry set <name>              # Set as primary registry
voss registry list                    # List all registries
voss registry login [spec]            # Show auth status / authenticate
voss registry register <pkg>@<ver>    # Publish package to registry
```

### Authentication

GitHub registries use `GITHUB_TOKEN` or `GH_TOKEN` environment variables:

```bash
export GITHUB_TOKEN=ghp_abc123def456
voss registry login github:myuser/packages
```

---

## 4. Version Resolution

### Version Specifiers

| Syntax | Example | Description |
|--------|---------|-------------|
| Exact | `1.0.0` | Match exact version |
| Caret | `^1.0.0` | Compatible with `>=1.0.0 <2.0.0` |
| Tilde | `~1.0.0` | Approximately `>=1.0.0 <1.1.0` |
| Greater | `>1.0.0` | Greater than |
| Less | `<2.0.0` | Less than |
| Range | `>=1.0.0 <2.0.0` | Inclusive/exclusive range |
| Wildcard | `*` | Any version |

### Lockfile (`aura.lock`)

```yaml
version: 1
packages:
  lodash:
    name: lodash
    version: 4.17.21
    resolved: npm:lodash@4.17.21
    integrity: sha256-abc123def456
    dependencies:
      - util-deprecate@1.0.2
  serde:
    name: serde
    version: 1.0.197
    resolved: cargo:serde@1.0.197
    integrity: sha256-789ghi012jkl
```

### Lock Commands

```bash
voss lock              # Resolve + lock all deps
voss freeze            # Pin all versions (generates aura.freeze)
voss unfreeze          # Unpin versions
voss update            # Update all deps to latest
```

---

## 5. Dependency Graph

```bash
voss tree              # Show dependency tree
voss graph             # Visual dependency graph
voss why <pkg>         # Why is this package installed?
voss verify            # Check for circular deps
voss dead              # Find unused dependencies
voss outdated          # Check for outdated packages
voss audit             # Scan for known vulnerabilities
```

---

## 6. Package Signing

Packages can be signed to verify authenticity and integrity.

### Sign a Package

```bash
voss sign my-package@1.0.0
```

Creates `packages/my-package/my-package@1.0.0.sig` containing a SHA-256 HMAC signature.

### Verify a Package

```bash
voss verify my-package@1.0.0
```

Checks the signature against the package name + version. Returns exit code 0 if valid.

### Automatic Verification

When installing packages, voss automatically checks for signatures:

```
$ voss install my-package
  resolving... found my-package@1.0.0
  signature OK
```

---

## 7. Sandboxing

### Running in Sandbox

```bash
voss sandbox my-package
```

Creates an isolated copy of the package at `.aura/sandbox/my-package/` with a sandbox policy:

```
sandbox policy:
  network:     isolated (no external access)
  filesystem:  read-only (packages/)
  process:     isolated
  memory:      512 MB limit
  cpu:         1 core
```

To run in the sandbox:

```bash
cd .aura/sandbox/my-package && voss run
```

### Permission System

Packages declare required permissions in `aurora.pkg`:

```
permissions:
  - network
  - filesystem
```

Manage permissions:

```bash
voss perms list           # List all permission groups
voss perms allow <perm>   # Allow a permission
voss perms deny <perm>    # Deny a permission
voss perms review         # Audit permissions
voss perms reset          # Reset permission policy
```

### Security Audit

```bash
voss audit                # Scan for vulnerabilities
voss audit --fix          # Auto-resolve lock file conflicts
voss trust <pkg>          # Show trust score
```

---

## 8. Testing & Benchmarking

### Run Tests

```bash
voss test
```

Discovers `tests/*.aura` files, compiles each with `aurorac`, and runs them. Reports pass/fail with timing.

```bash
$ voss test
running 3 test(s)...

  [test_math] PASS (12ms)
  [test_strings] PASS (8ms)
  [test_network] FAIL (exit=1, 45ms)

results: 3 total, 2 passed, 1 failed
```

### Benchmark

```bash
voss bench                        # Benchmark current package
voss bench my-package other-pkg   # Benchmark specific packages
```

Compiles the entry point, runs 10 iterations, reports average execution time:

```bash
$ voss bench
=== voss bench ===

my-app: 23.45 ms avg (10 runs)
```

---

## 9. Publishing

### Local Publish

```bash
voss publish
```

Stages the package as `.voss/name@version/` and creates a `.tgz` archive.

### GitHub Publish

```bash
voss publish github:user/repo
```

Creates a GitHub release with tag `v<version>`, uploads archive as asset, and pushes to remote.

### Registry Publish

```bash
voss registry register my-pkg@1.0.0 --registry github:myuser/packages
```

Or configure a registry first:

```bash
voss registry add my-gh github:myuser/packages
voss registry register my-pkg@1.0.0
```

---

## 10. Cross-Ecosystem Dependencies

Aurora can resolve dependencies across ecosystems (PyPI, npm, Cargo) using the `CrossEcosystemResolver`:

```yaml
# Example cross-ecosystem mappings
PyPI ⇄ npm:
  requests   ↔ node-fetch
  numpy      ↔ numjs
  pandas     ↔ danfojs
  flask      ↔ express

PyPI ⇄ Cargo:
  numpy      ↔ ndarray
  regex      ↔ regex
  pandas     ↔ polars

npm ⇄ Cargo:
  lodash     ↔ itertools
  moment     ↔ chrono
```

Import from existing package managers:

```bash
voss import npm     # Import from package.json
voss import pip     # Import from requirements.txt
voss import cargo   # Import from Cargo.toml
```

---

## 11. Advanced Features

### Cloud Builds

```bash
voss cloud-build init        # Initialize cloud build config
voss cloud-build run         # Submit reproducible build
voss cloud-build status      # Check build status
```

### Binary Distribution

```bash
voss binary package          # Package binaries
voss binary install <pkg>    # Install binary package
voss binary list             # List binary packages
```

### LTS Channels

```bash
voss lts list                # List LTS channels
voss lts add <name> <ver> <status>  # Add LTS channel
voss lts switch <channel>    # Switch to LTS channel
```

### Telemetry (Opt-In)

```bash
voss telemetry enable <endpoint>  # Enable telemetry
voss telemetry disable        # Disable telemetry
voss telemetry submit         # Submit telemetry data
```

### Fork Recovery

```bash
voss fork-recover <original> <fork>  # Replace package with fork
voss fork-list                # List active forks
```

### Lifecycle Monitor

```bash
voss lifecycle              # Show all lifecycle events
voss lifecycle <pkg>        # Show events for a package
```

### AI Package Generator

```bash
voss ai-gen "http client"   # Generate package from description
voss ai-gen "json parser" "output.json"  # Generate to file
```

---

## 12. Commands Reference

### Core

| Command | Description |
|---------|-------------|
| `voss init <name>` | Create new package |
| `voss install <pkg>` | Install dependency |
| `voss uninstall <pkg>` | Remove dependency |
| `voss lock` | Resolve + lock all deps |
| `voss build` | Compile |
| `voss run` | Build + execute |
| `voss test` | Run tests |
| `voss clean` | Clean build artifacts |
| `voss info` | Show package info |

### Analysis

| Command | Description |
|---------|-------------|
| `voss tree` | Show dependency tree |
| `voss graph` | Visual dependency graph |
| `voss verify` | Check circular deps |
| `voss audit` | Vulnerability scan |
| `voss why <pkg>` | Why is pkg installed? |
| `voss dead` | Find unused deps |
| `voss outdated` | Check outdated deps |
| `voss license` | Check licenses |

### Package Management

| Command | Description |
|---------|-------------|
| `voss publish` | Publish package |
| `voss update` | Update all deps |
| `voss freeze` | Pin versions |
| `voss unfreeze` | Unpin versions |
| `voss sign <pkg>` | Sign package |
| `voss verify <pkg>` | Verify signature |
| `voss search <q>` | Search packages |

### Security

| Command | Description |
|---------|-------------|
| `voss sandbox <pkg>` | Isolate package |
| `voss trust <pkg>` | Show trust score |
| `voss perms` | Manage permissions |
| `voss suggest <pkg>` | Package suggestions |

### Maintenance

| Command | Description |
|---------|-------------|
| `voss doctor` | Project diagnostics |
| `voss repair` | Self-healing |
| `voss import <fmt>` | Import from npm/pip/cargo |
| `voss migrate <pkg>@<ver>` | Upgrade package version |

### Advanced

| Command | Description |
|---------|-------------|
| `voss bench [pkgs]` | Benchmark |
| `voss bridge <eco> <pkg>` | Generate bridge binding |
| `voss snapshot` | Save/restore snapshots |
| `voss registry` | Registry management |
| `voss fork-recover` | Fork recovery |
| `voss ai-gen <desc>` | AI package generator |
| `voss cloud-build` | Cloud builds |
| `voss binary` | Binary distribution |

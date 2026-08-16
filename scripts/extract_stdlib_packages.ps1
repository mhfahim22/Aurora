# ═══════════════════════════════════════════════════════════════
# Phase 41.2 — Extract libc/*.auf into versioned std packages
# ═══════════════════════════════════════════════════════════════
# Creates packages/std-*/ directories, each a publishable Aurora
# package (aurora.pkg manifest + src/*.auf) wrapping a libc module.
# Usage: pwsh scripts/extract_stdlib_packages.ps1
# ═══════════════════════════════════════════════════════════════
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$libc = Join-Path $root 'libc'
$out  = Join-Path $root 'packages'

# name → (libc file base, description)
$packages = [ordered]@{
    'std-json'     = @{ file = 'json.auf';        desc = 'JSON parse/serialize library for Aurora' }
    'std-http'     = @{ file = 'server.auf';      desc = 'HTTP server framework for Aurora' }
    'std-db'       = @{ file = 'db.auf';          desc = 'SQLite database bindings for Aurora' }
    'std-net'      = @{ file = 'net.auf';         desc = 'TCP/UDP networking for Aurora' }
    'std-orm'      = @{ file = 'orm.auf';         desc = 'Object-relational mapping layer for Aurora' }
    'std-crypto'   = @{ file = 'crypto.auf';      desc = 'Cryptographic primitives (SHA-256, AES, HMAC)' }
    'std-regex'    = @{ file = 'regex.auf';       desc = 'Regular expression engine for Aurora' }
    'std-datetime' = @{ file = 'datetime.auf';    desc = 'Date/time types and formatting for Aurora' }
    'std-uuid'     = @{ file = 'uuid.auf';        desc = 'UUID v4 generation and parsing for Aurora' }
    'std-url'      = @{ file = 'url.auf';         desc = 'URL parsing, building and encoding for Aurora' }
    'std-decimal'  = @{ file = 'decimal.auf';     desc = 'Arbitrary-precision decimal arithmetic for Aurora' }
    'std-fs'       = @{ file = 'fs.auf';          desc = 'Filesystem operations for Aurora' }
    'std-collections' = @{ file = 'collections.auf'; desc = 'Data structures (list, map, set, queue) for Aurora' }
    'std-string'   = @{ file = 'string.auf';      desc = 'String utilities for Aurora' }
    'std-math'     = @{ file = 'math.auf';        desc = 'Mathematics functions for Aurora' }
}

$created = 0
foreach ($name in $packages.Keys) {
    $spec  = $packages[$name]
    $src   = Join-Path $libc $spec.file
    if (-not (Test-Path $src)) {
        Write-Host "skip $name — libc/$($spec.file) not found"
        continue
    }
    $pkgDir = Join-Path $out $name
    $srcDir = Join-Path $pkgDir 'src'
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null

    $ver = '1.0.0'
    Copy-Item -LiteralPath $src -Destination (Join-Path $srcDir $spec.file) -Force

    # Semantic version manifest (name uses the aurora/ namespace prefix)
    $manifest = "name: aurora/$name`nversion: $ver`nauthor: Aurora Core Team`ndescription: $($spec.desc)`nentry: src/$($spec.file)`ndependencies:`npermissions:`n"
    Set-Content -LiteralPath (Join-Path $pkgDir 'aurora.pkg') -Value $manifest -Encoding utf8

    # README
    $readme = "# aurora/$name`n`n$($spec.desc)`n`nSemantic version: $ver`n`n## Usage`n`nAurora packages install via voss:`n`n``````pwsh`nvoss install $name`n``````"
    Set-Content -LiteralPath (Join-Path $pkgDir 'README.md') -Value $readme -Encoding utf8

    $created++
}

Write-Host "extracted $created versioned std packages into packages/std-*"
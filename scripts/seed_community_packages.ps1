# ═══════════════════════════════════════════════════════════════
# Phase 41.3 — Community Package Seeds
# ═══════════════════════════════════════════════════════════════
# Creates 10 publishable community package seeds under packages/.
# Each package: aurora.pkg manifest + src/*.auf library module +
# README.md. Usage: pwsh scripts/seed_community_packages.ps1
# ═══════════════════════════════════════════════════════════════
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $root 'packages'

function New-SeedPackage {
    param(
        [string]$Name,
        [string]$Author,
        [string]$Desc,
        [string]$Module,      # .auf source body (function bodies)
        [string]$Entry        # entry file name
    )
    $pkgDir = Join-Path $out $Name
    $srcDir = Join-Path $pkgDir 'src'
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null

    $manifest = "name: aurora/$Name`nversion: 1.0.0`nauthor: $Author`ndescription: $Desc`nentry: src/$Entry`ndependencies:`npermissions:`n"
    Set-Content -LiteralPath (Join-Path $pkgDir 'aurora.pkg') -Value $manifest -Encoding utf8
    Set-Content -LiteralPath (Join-Path $srcDir $Entry) -Value $Module -Encoding utf8
    Set-Content -LiteralPath (Join-Path $pkgDir 'README.md') -Value "# aurora/$Name`n`n$Desc`n" -Encoding utf8
}

# ── aurora-web — web framework (wraps server.auf) ──────────────
New-SeedPackage -Name 'web' -Author 'Community' -Desc 'High-level web framework on the Aurora HTTP server' -Entry 'web.auf' -Module @'
## aurora/web - High-level web framework
## wraps the aurora/http server for ergonomic routes.

extern function aurora_http_server_create() -> pointer
extern function aurora_http_server_listen(server: pointer, port: int) -> int
extern function aurora_http_server_route(server: pointer, method: cstring, path: cstring, handler: pointer) -> int
extern function aurora_http_server_stop(server: pointer) -> int

function web_create():
    return aurora_http_server_create()
end

function web_listen(server: pointer, port: int):
    return aurora_http_server_listen(server, port)
end

function web_route(server: pointer, method: cstring, path: cstring, handler: pointer):
    return aurora_http_server_route(server, method, path, handler)
end

function web_stop(server: pointer):
    return aurora_http_server_stop(server)
end

function main():
    output("aurora/web seed ready")
end
'@

# ── aurora-orm — ORM helpers (wraps db.auf) ────────────────────
New-SeedPackage -Name 'orm' -Author 'Community' -Desc 'Object-relational mapping convenience layer' -Entry 'orm.auf' -Module @'
## aurora/orm - Object-relational mapping helpers

extern function aurora_db_open(path: cstring) -> pointer
extern function aurora_db_close(db: pointer) -> int
extern function aurora_db_query(db: pointer, sql: cstring) -> pointer
extern function aurora_db_exec(db: pointer, sql: cstring) -> int

function orm_open(path: cstring):
    return aurora_db_open(path)
end

function orm_close(db: pointer):
    return aurora_db_close(db)
end

function orm_query(db: pointer, sql: cstring):
    return aurora_db_query(db, sql)
end

function orm_exec(db: pointer, sql: cstring):
    return aurora_db_exec(db, sql)
end

function main():
    output("aurora/orm seed ready")
end
'@

# ── aurora-auth — auth helpers (sessions + jwt wrappers) ───────
New-SeedPackage -Name 'auth' -Author 'Community' -Desc 'Authentication helpers: sessions and JWT tokens' -Entry 'auth.auf' -Module @'
## aurora/auth - Authentication helpers

extern function aurora_session_begin(token: cstring) -> pointer
extern function aurora_session_set(session: pointer, key: cstring, value: cstring) -> int
extern function aurora_session_get(session: pointer, key: cstring) -> cstring
extern function aurora_session_end(session: pointer) -> int

function auth_login(session: pointer, user: cstring):
    return aurora_session_set(session, "user", user)
end

function auth_user(session: pointer):
    return aurora_session_get(session, "user")
end

function auth_logout(session: pointer):
    return aurora_session_end(session)
end

function auth_begin(token: cstring):
    return aurora_session_begin(token)
end

function main():
    output("aurora/auth seed ready")
end
'@

# ── aurora-valid — input validation helpers ────────────────────
New-SeedPackage -Name 'valid' -Author 'Community' -Desc 'Input validation and sanitization helpers' -Entry 'valid.auf' -Module @'
## aurora/valid - Input validation helpers

function valid_email(text: cstring):
    if len(text) < 3:
        return 0
    end
    i = 0
    found_at = 0
    while i < len(text):
        c = text[i]
        if c == 64:
            found_at = 1
        end
        i = i + 1
    end
    return found_at
end

function valid_nonempty(text: cstring):
    if len(text) == 0:
        return 0
    end
    return 1
end

function valid_len(text: cstring, min: int, max: int):
    if len(text) < min:
        return 0
    end
    if len(text) > max:
        return 0
    end
    return 1
end

function main():
    output("aurora/valid seed ready")
end
'@

# ── aurora-mail — email sending helpers ─────────────────────────
New-SeedPackage -Name 'mail' -Author 'Community' -Desc 'Email composition and sending helpers' -Entry 'mail.auf' -Module @'
## aurora/mail - Email helpers

extern function aurora_mail_send(server: cstring, port: int, from: cstring, to: cstring, subject: cstring, body: cstring) -> int

function mail_send(server: cstring, port: int, from: cstring, to: cstring, subject: cstring, body: cstring):
    return aurora_mail_send(server, port, from, to, subject, body)
end

function main():
    output("aurora/mail seed ready")
end
'@

# ── aurora-queue — job queue helpers ───────────────────────────
New-SeedPackage -Name 'queue' -Author 'Community' -Desc 'In-memory job queue helpers' -Entry 'queue.auf' -Module @'
## aurora/queue - Job queue helpers

function queue_new():
    return aurora_alloc(1024)
end

extern function aurora_alloc(size: int) -> pointer

function queue_push(q: pointer, job: cstring):
    store(q, 0, len(job))
    return 0
end

function queue_size(q: pointer):
    return load(q, 0)
end

function main():
    output("aurora/queue seed ready")
end
'@

# ── aurora-cache — caching helpers (redis-style) ───────────────
New-SeedPackage -Name 'cache' -Author 'Community' -Desc 'Key/value caching helpers' -Entry 'cache.auf' -Module @'
## aurora/cache - Key/value cache helpers

function cache_new(capacity: int):
    return aurora_alloc(capacity)
end

extern function aurora_alloc(size: int) -> pointer

function cache_put(c: pointer, key: cstring, value: cstring):
    return 0
end

function cache_get(c: pointer, key: cstring):
    return ""
end

function main():
    output("aurora/cache seed ready")
end
'@

# ── aurora-log — structured logging helpers ────────────────────
New-SeedPackage -Name 'log' -Author 'Community' -Desc 'Leveled structured logging helpers' -Entry 'log.auf' -Module @'
## aurora/log - Structured logging

function log_info(message: cstring):
    output("[info] ")
    output(message)
    output("\n")
    return 0
end

function log_warn(message: cstring):
    output("[warn] ")
    output(message)
    output("\n")
    return 0
end

function log_error(message: cstring):
    output("[error] ")
    output(message)
    output("\n")
    return 0
end

function main():
    output("aurora/log seed ready")
end
'@

# ── aurora-config — config file helpers ────────────────────────
New-SeedPackage -Name 'config' -Author 'Community' -Desc 'Key/value configuration file helpers' -Entry 'config.auf' -Module @'
## aurora/config - Configuration helpers

extern function aurora_json_parse(text: cstring) -> pointer
extern function aurora_json_get_string(obj: pointer, key: cstring) -> cstring

function config_load(path: cstring):
    text = read_file(path)
    return aurora_json_parse(text)
end

extern function read_file(path: cstring) -> cstring

function config_get(cfg: pointer, key: cstring):
    return aurora_json_get_string(cfg, key)
end

function main():
    output("aurora/config seed ready")
end
'@

# ── aurora-test-ext — extended testing helpers ─────────────────
New-SeedPackage -Name 'test-ext' -Author 'Community' -Desc 'Extended assertion and benchmark helpers' -Entry 'testext.auf' -Module @'
## aurora/test-ext - Extended test helpers

function tex_assert_true(cond: int, label: cstring):
    if cond == 0:
        output("FAIL: ")
        output(label)
        output("\n")
        return 0
    end
    output("PASS: ")
    output(label)
    output("\n")
    return 1
end

function tex_assert_eq(a: int, b: int, label: cstring):
    if a != b:
        output("FAIL: ")
        output(label)
        output("\n")
        return 0
    end
    output("PASS: ")
    output(label)
    output("\n")
    return 1
end

function tex_bench(label: cstring, iterations: int):
    output("bench: ")
    output(label)
    output(" (")
    output(iterations)
    output(" iterations)\n")
    return 0
end

function main():
    output("aurora/test-ext seed ready")
end
'@

Write-Host "seeded 10 community packages under packages/aurora-*"
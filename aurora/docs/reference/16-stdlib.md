# Standard Library (`libc/`)

The standard library provides ready-to-use FFI bindings and utility functions in `.auf` files.

## stdio.auf (C I/O)

| Function                         | Description              |
|----------------------------------|--------------------------|
| `printf(fmt, ...)`               | Print formatted          |
| `fprintf(stream, fmt, ...)`      | Print to stream          |
| `fopen(path, mode)`              | Open file                |
| `fclose(stream)`                 | Close file               |
| `fread(buf, size, count, s)`     | Read from file           |
| `fwrite(buf, size, count, s)`    | Write to file            |
| `fgets(buf, n, stream)`          | Read line                |
| `fputs(s, stream)`               | Write string             |
| `fflush(stream)`                 | Flush stream             |
| `fseek(stream, offset, origin)`  | Seek position            |
| `ftell(stream)`                  | Tell position            |
| `remove(path)`                   | Delete file              |
| `rename(old, new)`               | Rename file              |
| `puts(s)`                        | Print string with newline|
| `putchar(c)`                     | Print character          |
| `getchar()`                      | Read character           |

## stdlib.auf (C Standard Library)

| Function                         | Description              |
|----------------------------------|--------------------------|
| `malloc(size)`                   | Allocate memory          |
| `calloc(n, size)`                | Allocate zeroed memory   |
| `realloc(ptr, size)`             | Reallocate memory        |
| `free(ptr)`                      | Free memory              |
| `atoi(s)`                        | String to int            |
| `atol(s)`                        | String to long           |
| `atoll(s)`                       | String to long long      |
| `atof(s)`                        | String to float          |
| `strtol(s, end, base)`           | String to long with base |
| `rand()`                         | Random number            |
| `srand(seed)`                    | Seed random              |
| `exit(code)`                     | Exit process             |
| `abort()`                        | Abort process            |
| `atexit(fn)`                     | Register exit handler    |
| `abs(x)`                         | Absolute value           |
| `qsort(base, n, size, cmp)`      | Quick sort               |
| `bsearch(key, base, n, s, cmp)`  | Binary search            |
| `getenv(name)`                   | Environment variable     |
| `system(cmd)`                    | Run system command       |

## string.auf (C String + Utilities)

| Function                         | Description              |
|----------------------------------|--------------------------|
| `len(s)`                         | String length            |
| `strlen(s)`                      | C string length          |
| `strcat(dst, src)`               | Concatenate              |
| `substr(s, i, l)`                | Substring                |
| `index(s, sub)`                  | Find substring           |
| `has(s, sub)`                    | Contains                 |
| `starts(s, pre)`                 | Starts with              |
| `ends(s, suf)`                   | Ends with                |
| `upper(s)`                       | Uppercase                |
| `lower(s)`                       | Lowercase                |
| `trim(s)`                        | Trim whitespace          |
| `replace(s, a, b)`               | Replace                  |
| `split(s, d)`                    | Split                    |
| `join(arr, sep)`                 | Join                     |
| `reverse(s)`                     | Reverse                  |

## collections.auf

| Function                         | Description              |
|----------------------------------|--------------------------|
| `list`                           | Create list              |
| `list_get(l, i)`                 | Get element              |
| `list_len(l)`                    | List length              |
| `list_push(l, v)`                | Push element             |
| `list_last(l)`                   | Last element             |
| `list_first(l)`                  | First element            |
| `list_free(l)`                   | Free list                |
| `map`                            | Create map               |
| `map_get(m, k)`                  | Get value                |
| `map_has(m, k)`                  | Has key                  |
| `map_set(m, k, v)`               | Set value                |
| `map_free(m)`                    | Free map                 |
| `set`                            | Create set               |
| `set_add(s, v)`                  | Add value                |
| `set_has(s, v)`                  | Has value                |
| `set_free(s)`                    | Free set                 |
| `stack`                          | Create stack             |
| `stack_push(s, v)`               | Push                     |
| `stack_pop(s)`                   | Pop                      |
| `stack_empty(s)`                 | Is empty                 |
| `stack_free(s)`                  | Free stack               |
| `queue`                          | Create queue             |
| `queue_enqueue(q, v)`            | Enqueue                  |
| `queue_dequeue(q)`               | Dequeue                  |
| `queue_empty(q)`                 | Is empty                 |
| `queue_free(q)`                  | Free queue               |
| `vector`                         | Create 3D vector         |
| `vector_x(v)`                    | X component              |
| `vector_y(v)`                    | Y component              |
| `vector_z(v)`                    | Z component              |
| `json`                           | Create JSON value        |
| `json_free(j)`                   | Free JSON                |

## kernel32.auf (Windows API)

| Category     | Functions |
|--------------|-----------|
| **Time**     | `GetTickCount`, `Sleep`, `QueryPerformanceCounter`, `QueryPerformanceFrequency` |
| **Process**  | `ExitProcess`, `GetCurrentProcess`, `GetCurrentProcessId`, `TerminateProcess` |
| **Library**  | `LoadLibraryA/W`, `GetProcAddress`, `FreeLibrary` |
| **File I/O** | `CreateFileA/W`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize` |
| **Console**  | `GetStdHandle`, `WriteConsoleA/W`, `ReadConsoleA/W`, `SetConsoleTitle` |
| **Memory**   | `VirtualAlloc`, `VirtualFree`, `HeapAlloc`, `HeapFree` |
| **Env**      | `GetEnvironmentVariableA/W`, `SetEnvironmentVariableA/W`, `GetCommandLineA/W` |
| **System**   | `GetSystemInfo`, `GetComputerNameA/W`, `GetUserNameA/W` |
| **Error**    | `GetLastError`, `FormatMessageA/W` |
| **Fibers**   | `ConvertThreadToFiber`, `CreateFiber`, `SwitchToFiber`, `DeleteFiber` |

## user32.auf (Windows GUI)

Message boxes, window management (`CreateWindowEx`, `ShowWindow`, `UpdateWindow`), window class registration, message loop (`GetMessage`, `DispatchMessage`, `TranslateMessage`), drawing (`BeginPaint`, `EndPaint`, `InvalidateRect`), resources, timers, clipboard, dialogs.

## gui.auf (Cross-platform)

| Function                                  | Description              |
|-------------------------------------------|--------------------------|
| `gui_window_new(title, w, h)`             | Create window            |
| `gui_button_new(parent, text, x, y, w, h)`| Create button            |
| `gui_label_new(parent, text, x, y, w, h)` | Create label             |
| `gui_textbox_new(parent, x, y, w, h)`     | Create textbox           |
| `gui_listbox_new(parent, x, y, w, h)`     | Create listbox           |
| `gui_run()`                               | Run event loop           |
| `gui_quit()`                              | Quit event loop          |

## libtorch.auf (PyTorch C++)

| Function                  | Description              |
|---------------------------|--------------------------|
| `tensor_1d/2d/3d(data)`   | Create tensor            |
| `tensor_add/sub/mul/div`  | Element-wise ops         |
| `tensor_matmul(a, b)`     | Matrix multiply          |
| `tensor_relu/sigmoid/tanh/softmax` | Activations   |
| `module_load(path)`       | Load model               |
| `module_forward(m, input)`| Forward pass             |
| `module_destroy(m)`       | Unload model             |
| `optimizer_sgd(m, lr)`    | SGD optimizer            |

## pq.auf (PostgreSQL libpq)

Connection management, query execution, result handling, prepared statements, async query support, COPY operations, large object support.

Constants: `PGRES_COMMAND_OK`, `PGRES_TUPLES_OK`, etc.

## serial.auf (Serialization — Phase 14)

Serialization supports JSON and binary (TLV) formats with auto-detection.

| Function | Description |
|----------|-------------|
| `serial_json_encode(value)` | Encode value to JSON string |
| `serial_json_decode(str)` | Decode JSON string to value |
| `serial_binary_encode(value)` | Encode value to binary blob |
| `serial_binary_decode(data)` | Decode binary blob |
| `serial_write_json(path, val)` | Write JSON to file |
| `serial_read_json(path)` | Read JSON from file |
| `serial_write_binary(path, val)` | Write binary to file |
| `serial_read_binary(path)` | Read binary from file |
| `serial_detect_format(path)` | Auto-detect JSON or binary |

## db.auf (SQLite3 Database — Phase 15)

SQLite3 database API via `sqlite://` URL scheme.

| Function | Description |
|----------|-------------|
| `db_open(path)` | Open database connection |
| `db_close(db)` | Close connection |
| `db_exec(db, sql)` | Execute SQL |
| `db_query(db, sql)` | Query rows as array |
| `db_prepare(db, sql)` | Prepare statement |
| `db_bind_int(stmt, idx, val)` | Bind integer parameter |
| `db_bind_float(stmt, idx, val)` | Bind float parameter |
| `db_bind_text(stmt, idx, val)` | Bind text parameter |
| `db_bind_null(stmt, idx)` | Bind NULL |
| `db_step(stmt)` | Advance statement |
| `db_column_count(stmt)` | Get column count |
| `db_column_type(stmt, idx)` | Get column type |
| `db_column_int(stmt, idx)` | Get column as int |
| `db_column_float(stmt, idx)` | Get column as float |
| `db_column_text(stmt, idx)` | Get column as text |
| `db_finalize(stmt)` | Finalize statement |
| `db_last_id(db)` | Last insert row ID |
| `db_changes(db)` | Rows changed |
| `db_begin(db)` | Begin transaction |
| `db_commit(db)` | Commit transaction |
| `db_rollback(db)` | Rollback transaction |
| `db_escape(str)` | Escape SQL string |
| `db_error(db)` | Get error message |

## android.auf (Android Runtime — Phase 16)

Android platform bindings (available on Android only).

| Function | Description |
|----------|-------------|
| `android_toast(text)` | Show toast notification |
| `android_vibrate(ms)` | Vibrate device |
| `android_get_sdk()` | Get SDK version |
| `android_get_model()` | Get device model |
| `android_get_brand()` | Get device brand |
| `android_has_permission(name)` | Check permission |
| `android_request_permission(name)` | Request permission |
| `android_get_external_storage()` | External storage path |
| `android_get_internal_storage()` | Internal storage path |
| `android_get_cache_dir()` | Cache directory |
| `android_start_activity(action, uri)` | Start activity |
| `android_share_text(text)` | Share text |
| `android_open_url(url)` | Open URL in browser |
| `android_activity_name()` | Activity class name |
| `android_finish()` | Close activity |
| `android_get_data_dir()` | Data directory |

## ios.auf (iOS Runtime — Phase 16)

iOS platform bindings (available on iOS only).

| Function | Description |
|----------|-------------|
| `ios_alert(title, msg)` | Show alert |
| `ios_vibrate()` | Vibrate device |
| `ios_get_os_version()` | iOS version string |
| `ios_get_device_model()` | Device model |
| `ios_get_bundle_id()` | Bundle identifier |
| `ios_get_bundle_version()` | Bundle version |
| `ios_get_bundle_name()` | Bundle name |
| `ios_get_documents_dir()` | Documents directory |
| `ios_get_cache_dir()` | Cache directory |
| `ios_open_url(url)` | Open URL |
| `ios_share(items)` | Share sheet |
| `ios_haptic_light()` | Light haptic feedback |
| `ios_haptic_medium()` | Medium haptic feedback |
| `ios_haptic_heavy()` | Heavy haptic feedback |

## mobile_widgets.auf (Mobile Widgets — Phase 17)

Cross-platform widget engine for mobile UIs.

| Function | Description |
|----------|-------------|
| `mw_create(wtype, parent)` | Create widget |
| `mw_destroy(widget)` | Destroy widget |
| `mw_set_pos(w, x, y)` | Set position |
| `mw_set_size(w, w_, h_)` | Set size |
| `mw_set_text(w, text)` | Set text content |
| `mw_set_color(w, color)` | Set background color |
| `mw_set_visible(w, visible)` | Show/hide |
| `mw_set_opacity(w, opacity)` | Set opacity |
| `mw_set_radius(w, radius)` | Set corner radius |
| `mw_add_child(parent, child)` | Add child widget |
| `mw_remove_child(parent, child)` | Remove child |
| `mw_layout(w)` | Layout children |
| `mw_hit_test(w, x, y)` | Hit test point |
| `mw_on_event(w, type, cb)` | Register event callback |
| `mw_dispatch(w, event)` | Dispatch event |
| `mw_set_data(w, key, val)` | Set user data |
| `mw_get_data(w, key)` | Get user data |
| `mw_set_layout_type(w, type)` | Set layout (column/row/grid) |
| `mw_set_layout_spacing(w, s)` | Set spacing |
| `mw_set_layout_padding(w, l, t, r, b)` | Set padding |
| `mw_set_layout_margin(w, l, t, r, b)` | Set margin |
| `mw_scroll_to(w, x, y)` | Scroll content |
| `mw_get_scroll_x(w)` | Get scroll offset X |
| `mw_get_scroll_y(w)` | Get scroll offset Y |
| `mw_focus(w)` | Focus widget |
| `mw_blur(w)` | Blur widget |
| `mw_render(w)` | Trigger render |

## desktop.auf (Desktop Integration — Phase 18)

Win32 desktop features.

| Function | Description |
|----------|-------------|
| `desktop_tray_init(wnd, icon, tip)` | Initialize system tray |
| `desktop_tray_set_icon(icon)` | Set tray icon |
| `desktop_tray_set_tip(tip)` | Set tray tooltip |
| `desktop_tray_add_menu(text, id)` | Add context menu item |
| `desktop_tray_show_balloon(title, msg, icon, timeout)` | Show balloon notification |
| `desktop_tray_remove()` | Remove tray icon |
| `desktop_notify(title, msg)` | Show notification |
| `desktop_clipboard_get_text()` | Get clipboard text |
| `desktop_clipboard_set_text(text)` | Set clipboard text |
| `desktop_dnd_enable(wnd)` | Enable drag & drop |
| `desktop_assoc_set(ext, prog_id, desc, cmd)` | Set file association |
| `desktop_assoc_remove(ext)` | Remove file association |
| `desktop_startup_set(name, path)` | Register startup app |
| `desktop_startup_remove(name)` | Remove startup app |
| `desktop_effects_acrylic(wnd, color)` | Enable acrylic |
| `desktop_effects_mica(wnd)` | Enable mica |
| `desktop_effects_blur(wnd)` | Enable blur-behind |
| `desktop_effects_dark_mode(wnd, enable)` | Toggle dark title bar |
| `desktop_effects_rounded(wnd, enable)` | Toggle rounded corners |
| `desktop_hotkey_register(id, mod, key)` | Register global hotkey |
| `desktop_hotkey_unregister(id)` | Unregister hotkey |

## game.auf (OpenGL & Game — Phase 19)

Lighting, tilemaps, and mesh primitives.

| Function | Description |
|----------|-------------|
| `light_create(r, g, b)` | Create light |
| `light_destroy(id)` | Destroy light |
| `light_set_position(id, x, y, z)` | Set position |
| `light_set_direction(id, x, y, z)` | Set direction |
| `light_set_color(id, r, g, b)` | Set color |
| `light_set_intensity(id, intensity)` | Set intensity |
| `light_set_range(id, range)` | Set range |
| `light_set_spot_angle(id, angle)` | Set spot angle |
| `light_get_count()` | Get light count |
| `light_get(index)` | Get light params |
| `tilemap_create(rows, cols, layers)` | Create tilemap |
| `tilemap_destroy(tm)` | Destroy tilemap |
| `tilemap_set_tile(tm, row, col, layer, val)` | Set tile |
| `tilemap_get_tile(tm, row, col, layer)` | Get tile |
| `tilemap_get_width(tm)` | Get tilemap width |
| `tilemap_get_height(tm)` | Get tilemap height |
| `tilemap_get_rows(tm)` | Get row count |
| `tilemap_get_cols(tm)` | Get col count |
| `tilemap_is_solid(tm, row, col)` | Check solid tile |
| `tilemap_set_property(tm, key, val)` | Set tilemap property |
| `mesh_create_plane()` | Create plane mesh |
| `mesh_create_sphere(lat, lon)` | Create sphere mesh |
| `mesh_create_cylinder(segments)` | Create cylinder mesh |
| `mesh_create_capsule(segments)` | Create capsule mesh |
| `mesh_get_vertex_count(mesh)` | Get vertex count |
| `mesh_get_vertex_data(mesh)` | Get vertex data |
| `mesh_get_index_count(mesh)` | Get index count |
| `mesh_get_index_data(mesh)` | Get index data |
| `mesh_destroy(mesh)` | Destroy mesh |

## plugin.auf (Plugin System — Phase 20)

Native plugin loading and reflection.

| Function | Description |
|----------|-------------|
| `plugin_load(path)` | Load plugin |
| `plugin_unload(id)` | Unload plugin |
| `plugin_scan(dir)` | Scan directory for plugins |
| `plugin_count()` | Get loaded plugin count |
| `plugin_name(id)` | Get plugin name |
| `plugin_version(id)` | Get plugin version |
| `plugin_author(id)` | Get plugin author |
| `plugin_description(id)` | Get plugin description |
| `plugin_has_init(id)` | Check init function |
| `plugin_call_init(id)` | Call plugin init |
| `plugin_call_shutdown(id)` | Call plugin shutdown |
| `plugin_abi_version(id)` | Get plugin ABI version |
| `plugin_type_count(id)` | Get type count |
| `plugin_type_name(id, idx)` | Get type name |
| `plugin_field_count(id, type_idx)` | Get field count |
| `plugin_field_info(id, type_idx, field_idx)` | Get field info |
| `plugin_method_count(id, type_idx)` | Get method count |
| `plugin_method_name(id, type_idx, method_idx)` | Get method name |

## pkg.auf (Package Manager — Phase 21)

Package management via the Voss CLI.

| Function | Description |
|----------|-------------|
| `pkg_install(name)` | Install package |
| `pkg_remove(name)` | Remove package |
| `pkg_update(name)` | Update package |
| `pkg_publish(path)` | Publish package |
| `pkg_search(query)` | Search packages |
| `pkg_list_installed()` | List installed packages |
| `pkg_set_registry(url)` | Set registry URL |
| `pkg_registry_url()` | Get registry URL |
| `pkg_login(token)` | Login with token |
| `pkg_logout()` | Logout |
| `pkg_lock_init()` | Init lock file |
| `pkg_lock_save()` | Save lock file |
| `pkg_lock_load()` | Load lock file |
| `pkg_resolve(name)` | Resolve dependency |
| `pkg_resolve_count()` | Get dep count |
| `pkg_resolve_name(idx)` | Get dep name |
| `pkg_cache_list()` | List cached packages |
| `pkg_cache_clear()` | Clear cache |

## hotreload.auf (Hot Reload — Phase 23)

File watching and live reload.

| Function | Description |
|----------|-------------|
| `hr_watch(path)` | Watch file/directory |
| `hr_unwatch(path)` | Stop watching |
| `hr_on_ui_change(callback)` | UI reload callback |
| `hr_on_code_change(callback)` | Code reload callback |
| `hr_on_asset_change(callback)` | Asset reload callback |
| `hr_code_reload()` | Trigger code reload |
| `hr_code_version()` | Get code version |
| `hr_code_stale()` | Check staleness |
| `hr_asset_reload(path)` | Reload asset |
| `hr_asset_is_dirty(path)` | Check dirty |
| `hr_asset_clear_dirty(path)` | Clear dirty flag |
| `hr_state_set(key, val)` | Save state |
| `hr_state_get(key)` | Load state |
| `hr_state_has(key)` | Check state key |
| `hr_state_remove(key)` | Remove state |
| `hr_console_log(msg)` | Log to console |
| `hr_console_clear()` | Clear console |
| `hr_console_get(count)` | Get recent logs |
| `hr_console_exec(cmd)` | Execute console command |

## test.auf (Testing Framework — Phase 24)

Unit, integration, and benchmark testing.

| Function | Description |
|----------|-------------|
| `test_suite(name)` | Register test suite |
| `test_case(name)` | Register test case |
| `test_run(results)` | Run tests |
| `assert_true(cond)` | Assert true |
| `assert_false(cond)` | Assert false |
| `assert_eq(actual, expected)` | Assert equal |
| `assert_neq(a, b)` | Assert not equal |
| `assert_gt(a, b)` | Assert greater than |
| `assert_lt(a, b)` | Assert less than |
| `assert_null(val)` | Assert null |
| `integration_setup(callback)` | Integration setup |
| `integration_teardown(callback)` | Integration teardown |
| `integration_test(name, callback)` | Integration test |
| `widget_test(name, callback)` | Widget test |
| `widget_setup(width, height)` | Widget test setup |
| `widget_click(x, y)` | Simulate click |
| `bench_start(suite)` | Start benchmark |
| `bench_end(suite)` | End benchmark |
| `bench_report(suite)` | Get benchmark report |
| `snap_shot(name, value)` | Capture snapshot |
| `snap_assert(name)` | Assert snapshot matches |
| `snap_clear(name)` | Clear snapshot |
| `coverage_start()` | Start coverage |
| `coverage_stop()` | Stop coverage |
| `coverage_report()` | Get coverage report |
| `test_pass_count()` | Get pass count |
| `test_fail_count()` | Get fail count |
| `test_total_count()` | Get total count |
| `test_results()` | Get results summary |

## dev.auf (Developer Tools — Phase 25)

Formatting, linting, completions, profiling, and debugging utilities.

| Function | Description |
|----------|-------------|
| `format_code(code)` | Format source code |
| `format_file(path)` | Format file in-place |
| `set_tab_size(n)` | Set formatter tab size |
| `set_use_spaces(v)` | Toggle spaces vs tabs |
| `lint(code)` | Lint source code |
| `lint_file(path)` | Lint file |
| `set_lint_rule(r, e)` | Enable/disable rule |
| `lsp_start(port)` | Start LSP server |
| `lsp_stop()` | Stop LSP server |
| `lsp_is_running()` | Check LSP status |
| `lsp_set_root(path)` | Set LSP workspace root |
| `complete(code, line, col)` | Get completions |
| `complete_file(path, line, col)` | Get file completions |
| `complete_detail(label)` | Get completion detail |
| `debug_attach(target)` | Attach debugger |
| `debug_break()` | Break execution |
| `debug_continue()` | Continue execution |
| `debug_step_over()` | Step over |
| `profiler_start()` | Start profiling |
| `profiler_stop()` | Stop profiling |
| `profiler_report()` | Get profile report |
| `profiler_reset()` | Reset profiler data |
| `inspector_tree()` | Get widget tree |
| `inspector_select(x, y)` | Select widget at point |
| `inspector_refresh()` | Refresh inspector |
| `memory_stats()` | Get memory stats |
| `memory_snapshot()` | Take memory snapshot |
| `memory_leak_check()` | Check for leaks |
| `perf_start()` | Start perf monitor |
| `perf_stop()` | Stop perf monitor |
| `perf_fps()` | Get current FPS |
| `perf_frame_time()` | Get frame time |
| `perf_report()` | Get perf report |

## security.auf (Security — Phase 27)

Sandbox, permissions, encryption, certificates, hashing, and authentication.

| Function | Description |
|----------|-------------|
| `sandbox_init()` | Initialize sandbox |
| `sandbox_allow_path(path)` | Allow a path in sandbox |
| `sandbox_check_path(path)` | Check if path is allowed |
| `sandbox_destroy()` | Destroy sandbox |
| `permission_check(perm)` | Check permission |
| `permission_request(perm)` | Request permission |
| `permission_list()` | List granted permissions |
| `permission_revoke(perm)` | Revoke permission |
| `storage_open(path, key, key_len)` | Open encrypted key-value store |
| `storage_set(store, key, value)` | Set encrypted value |
| `storage_get(store, key)` | Get decrypted value |
| `storage_remove(store, key)` | Remove key |
| `storage_close(store)` | Close store |
| `generate_key(out, len)` | Generate random key |
| `generate_iv(out, len)` | Generate random IV |
| `encrypt(key, key_len, iv, input, in_len, output, out_len)` | AES-CBC encrypt |
| `decrypt(key, key_len, iv, input, in_len, output, out_len)` | AES-CBC decrypt |
| `pbkdf2(password, salt, salt_len, iter, out, out_len)` | Password-based key derivation |
| `cert_load(path)` | Load certificate |
| `cert_info(cert)` | Get certificate info |
| `cert_verify(cert, ca_path)` | Verify certificate chain |
| `cert_free(cert)` | Free certificate |
| `sha256(data, len)` | SHA-256 hash (hex) |
| `hmac_sha256(key, key_len, data, data_len)` | HMAC-SHA256 (hex) |
| `hash_password(password)` | Hash password with salt |
| `verify_password(password, hash)` | Verify password against hash |
| `token_generate(payload, secret)` | Generate auth token |
| `token_verify(token, secret)` | Verify auth token |
| `basic_auth(username, password)` | Generate Basic auth header |
| `bearer_auth(token)` | Generate Bearer auth header |

---

**Next:** [Frameworks](17-frameworks.md)

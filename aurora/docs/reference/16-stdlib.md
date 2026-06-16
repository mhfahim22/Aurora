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

Time: `GetTickCount`, `Sleep`, `QueryPerformanceCounter`, `QueryPerformanceFrequency`  
Process: `ExitProcess`, `GetCurrentProcess`, `GetCurrentProcessId`, `TerminateProcess`  
Library: `LoadLibraryA/W`, `GetProcAddress`, `FreeLibrary`  
File I/O: `CreateFileA/W`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize`  
Console: `GetStdHandle`, `WriteConsoleA/W`, `ReadConsoleA/W`, `SetConsoleTitle`  
Memory: `VirtualAlloc`, `VirtualFree`, `HeapAlloc`, `HeapFree`  
Environment: `GetEnvironmentVariableA/W`, `SetEnvironmentVariableA/W`, `GetCommandLineA/W`  
System: `GetSystemInfo`, `GetComputerNameA/W`, `GetUserNameA/W`  
Error: `GetLastError`, `FormatMessageA/W`  
Fibers: `ConvertThreadToFiber`, `CreateFiber`, `SwitchToFiber`, `DeleteFiber`

## user32.auf (Windows GUI)

Message boxes, window management (CreateWindowEx, ShowWindow, UpdateWindow),  
window class registration, message loop (GetMessage, DispatchMessage, TranslateMessage),  
drawing (BeginPaint, EndPaint, InvalidateRect), resources, timers, clipboard, dialogs.

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

Connection management, query execution, result handling, prepared statements,  
async query support, COPY operations, large object support.  

Constants: `PGRES_COMMAND_OK`, `PGRES_TUPLES_OK`, etc.

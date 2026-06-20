# Built-in Functions

## I/O

| Function         | Description                     |
|------------------|---------------------------------|
| `output(expr)`   | Print value                     |
| `output expr`    | Print value (no parens)         |
| `outputln(expr)` | Print with newline              |
| `outputN()`      | Print newline only              |
| `outputf(fmt..)` | Formatted output                |
| `input()`        | Read input                      |
| `ask(prompt)`    | Prompt and read input           |
| `debug expr`     | Debug print                     |
| `log expr`       | Log message                     |

```aura
output("hello")                   # prints: hello
output x                          # no-paren form
outputln("line")                  # with newline
outputN()                         # just newline
outputf("Sum: %d\n", total)       # formatted
```

> **`debug` vs `log`:** `debug` prints with source location info; `log` adds a timestamp prefix.

## String

| Function           | Description              |
|--------------------|--------------------------|
| `len(s)`           | String length            |
| `upper(s)`         | Uppercase                |
| `lower(s)`         | Lowercase                |
| `trim(s)`          | Trim whitespace          |
| `replace(s, a, b)` | Replace substring        |
| `split(s, d)`      | Split by delimiter       |
| `join(arr, sep)`   | Join array               |
| `has(s, sub)`      | Contains substring       |
| `starts(s, pre)`   | Starts with prefix       |
| `ends(s, suf)`     | Ends with suffix         |
| `reverse(s)`       | Reverse string           |
| `substr(s, i, l)`  | Substring at index       |
| `index(s, sub)`    | Find substring index     |

## Math

| Function           | Description              |
|--------------------|--------------------------|
| `abs(x)`           | Absolute value           |
| `sqrt(x)`          | Square root              |
| `floor(x)`         | Round down               |
| `ceil(x)`          | Round up                 |
| `round(x)`         | Round nearest            |
| `pow(x, y)`        | Power                    |
| `clamp(x, lo, hi)` | Clamp value              |
| `rand()`           | Random number            |
| `sum(arr)`         | Sum array                |
| `min(a, b)`        | Minimum                  |
| `max(a, b)`        | Maximum                  |

## Type Conversion & Reflection

| Function     | Description              |
|--------------|--------------------------|
| `str(x)`     | Convert to string        |
| `int(x)`     | Convert to integer       |
| `float(x)`   | Convert to float         |
| `bool(x)`    | Convert to boolean       |
| `char(x)`    | Convert to character     |
| `convert(x)` | Type coercion            |
| `clone(x)`   | Deep copy                |
| `typeof(x)`  | Get type name            |
| `sizeof(x)`  | Get size in bytes        |
| `fields(x)`  | List struct/class fields |
| `methods(x)` | List object methods      |

## Collections

| Function             | Description                   |
|----------------------|-------------------------------|
| `push(arr, val)`     | Append element                |
| `pop(arr)`           | Remove last element           |
| `insert(arr, i, v)`  | Insert at index               |
| `remove(arr, i)`     | Remove at index               |
| `clear(arr)`         | Clear all elements            |
| `sort(arr)`          | Sort in place                 |
| `unique(arr)`        | Remove duplicates             |
| `map(arr, fn)`       | Transform elements            |
| `filter(arr, fn)`    | Filter elements               |
| `reduce(arr, fn, i)` | Reduce to single value        |
| `find(arr, val)`     | Find element index            |
| `any(arr, fn)`       | Any element matches           |
| `all(arr, fn)`       | All elements match            |
| `range(start, end)`  | Generate range                |

## Runtime Collections

| Function                  | Description              |
|---------------------------|--------------------------|
| `list_get(l, i)`          | List element at index    |
| `list_len(l)`             | List length              |
| `list_push(l, v)`         | Push to list             |
| `list_free(l)`            | Free list                |
| `map_get(m, k)`           | Map value by key         |
| `map_has(m, k)`           | Map has key              |
| `map_set(m, k, v)`        | Set map value            |
| `map_free(m)`             | Free map                 |
| `set_add(s, v)`           | Add to set               |
| `set_has(s, v)`           | Set has value            |
| `set_free(s)`             | Free set                 |
| `stack_push(s, v)`        | Push to stack            |
| `stack_pop(s)`            | Pop from stack           |
| `stack_empty(s)`          | Stack is empty           |
| `stack_free(s)`           | Free stack               |
| `queue_enqueue(q, v)`     | Enqueue                  |
| `queue_dequeue(q)`        | Dequeue                  |
| `queue_empty(q)`          | Queue is empty           |
| `queue_free(q)`           | Free queue               |

## File I/O

| Function                  | Description              |
|---------------------------|--------------------------|
| `read(path)`              | Read file contents       |
| `write(path, data)`       | Write file               |
| `append(path, data)`      | Append to file           |
| `exists(path)`            | File exists              |
| `delete(path)`            | Delete file              |
| `copy(src, dst)`          | Copy file                |
| `move(src, dst)`          | Move file                |
| `download(url, path)`     | Download file            |

## Path

| Function     | Description              |
|--------------|--------------------------|
| `cwd()`      | Current working directory|
| `cd(path)`   | Change directory         |
| `path(p)`    | Parse path               |
| `name(p)`    | File name from path      |
| `ext(p)`     | Extension from path      |
| `dir(p)`     | Directory from path      |

## Time

| Function     | Description              |
|--------------|--------------------------|
| `now()`      | Current time             |
| `sleep(ms)`  | Sleep milliseconds       |
| `stamp()`    | Timestamp                |

## OS / Environment

| Function     | Description              |
|--------------|--------------------------|
| `os()`       | OS name                  |
| `cpu()`      | CPU info                 |
| `mem()`      | Memory info              |
| `env(name)`  | Environment variable     |
| `run(cmd)`   | Run command              |
| `exit(code)` | Exit process             |

## JSON

| Function        | Description              |
|-----------------|--------------------------|
| `encode(val)`   | Encode to JSON           |
| `decode(str)`   | Decode from JSON         |

## HTTP

| Function                | Description              |
|-------------------------|--------------------------|
| `get(url)`              | HTTP GET                 |
| `post(url, data)`       | HTTP POST                |

## Performance

| Function     | Description              |
|--------------|--------------------------|
| `measure(fn)`| Measure execution time   |
| `bench(fn)`  | Benchmark function       |
| `profile(fn)`| Profile execution        |
| `trace(fn)`  | Trace execution          |

## Other Built-in Keywords

| Keyword      | Description              |
|--------------|--------------------------|
| `time`       | Current timestamp        |
| `random`     | Random number            |
| `sleep expr` | Sleep (statement form)   |
| `pass`       | No-op                    |

---

**Next:** [Standard Library](16-stdlib.md)

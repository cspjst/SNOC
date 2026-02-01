## 2. Patterns and Pattern Functions

In `sno`, a pattern is not a compiled regular expression or a data structure—it is an ordinary C function that matches text and advances a cursor. These pattern functions are the atomic building blocks of parsing, each consumes zero or more characters from the current position, returns success or failure, and leaves the parser in a well-defined state.

### SNOBOL Heritage, C Execution Model

`sno` draws direct inspiration from SNOBOL4's pattern algebra—LEN, SPAN, BREAK—but adapts it to C's imperative semantics:

| Aspect                 | SNOBOL                                 | `sno`                                |
| ---------------------- | -------------------------------------- | ------------------------------------ |
| Pattern representation | First-class value (`pattern = LEN(4)`) | Ordinary function `(sno_len(&s, 4))` |
| Matching               | Separate operation (`subject pattern`) | Immediate execution (function call)  |
| Composition            | Concatenation / alternation operators  | Native C `&&` /  ||`                 |
| State                  | Implicit cursor in pattern matcher     | Explicit cursor in `sno_subject_t`   |

This eliminates SNOBOL's pattern compilation step while preserving its compositional elegance. You call pattern functions directly—no intermediate representation, no hidden engine.

### The View: Cursor and Extraction Combined

SNOBOL maintains an implicit cursor position during matching. To capture matched text, you must explicitly assign it using the `.VAR` operator *after* the match succeeds:

```
1
```

`sno` improves this model by unifying cursor and extraction into a single concept: the **view** (`sno_view_t`). After any successful pattern match, `s.view` is a half-open span `[begin, end)` that:

1. Represents the current cursor position (`s.view.end` is where the next match begins)
2. *Is* the extracted substring—no separate assignment step required

```
c













1

2

3

4
```

This design eliminates SNOBOL's two-step dance (match then assign). The view is both position and payload—zero-copy by default, with explicit extraction (`sno_var`) available only when null-termination is required by legacy APIs.

### Expressive Power Without Backtracking

Pattern functions operate at the intersection of Chomsky Type 3 (regular) and practical Type 2 (context-free) languages:

- **Type 3**: Fixed-width fields (`sno_len`), character classes (`sno_span`, `sno_break`), and sequences compose naturally with `&&`/`||`.
- **Practical Type 2**: The explicit cursor model enables helpers for balanced delimiters (parentheses, quotes) without regex-style backtracking.

Critically, `sno` avoids regex's pitfalls:

- ✅ No backtracking explosions—each primitive advances the cursor deterministically
- ✅ No hidden state—the entire parser state is `s.view.end`
- ✅ O(n) worst-case—pointer arithmetic only, no stack unwinding

### The Primitive Contract

All pattern functions share a simple contract:

```
c













1
```

- **Success**: cursor (`s.view.end`) advances; `s.view` holds the exact matched span
- **Failure**: cursor and view unchanged (enables safe `||` alternation)
- **Composition**: chain with `&&` for sequences, `||` for alternatives

These primitives form a minimal core—every higher-level parser (CSV, INI, log formats) is built by composing them with native C control flow. No macros. No DSL. Just functions that transform cursor state predictably, with extraction built in.

### 2.1`sno_bind` — Establish Subject String

`sno_bind(s, str)` binds the null‑terminated immutable string `str` to parsing context `s`. The function computes the string length once, caches it in `s->length`, and initializes the cursor to the start of the string with an empty view span `[str, str)`.

After binding, all pattern operations (`sno_lit`, `sno_len`, etc.) operate on this subject until a new string is bound. The subject string must remain valid for the duration of parsing—`sno` never copies or owns the string data.

###### Example — Prepare to Parse a Line

```c
sno_subject_t s = {0};
sno_bind(&s, "host=alpha");
```

The parser is now ready to match patterns from the beginning of the string.

###### Example — Rebind to New Input

```c
sno_bind(&s, "port=8080");   /* Reuse same context for next line */
```

Binding a new string resets all parser state—no need to reallocate or reinitialize the `sno_subject_t` structure. This makes `sno` ideal for line‑by‑line processing of streams or files.

`sno_bind` establishes the foundation upon which all pattern matching occurs: an immutable subject string, a cached length for O(1) bounds checking, and a cursor positioned at the start. Every subsequent primitive (`sno_lit`, `sno_len`, etc.) transforms this state predictably—advancing the cursor on success, leaving it unchanged on failure—enabling composable, fail‑fast parsing in C.

### 2.2`sno_lit` — Single Character Matching

`sno_lit(s, ch)` literal character matches exactly one character `ch` at the current cursor position in subject string `s`. 

- **Success**: the cursor advances by one position and `s->view` becomes a one‑character span `[old_cursor, old_cursor+1)` containing the matched character. The function returns `true`.
- **Failure**: both cursor and view remain unchanged. The function returns `false`.

This primitive is the building block for delimiter recognition, comment detection, and simple token parsing—all without allocations or hidden state.

###### Example — Detect Shebang Lines (`#!`)

Identify Unix executable scripts by their leading `#!` sequence:

```c
sno_bind(&s, "#!/bin/sh");

if (sno_lit(&s, '#') && sno_lit(&s, '!')) {
    printf("Shebang interpreter: %s\n", s.view.end); 
}
```

###### Output:

```
Shebang interpreter: /bin/sh
```

### 2.3`sno_len` — Fixed-Length Matching

sno_len(s, n)` matches exactly `n` characters starting from the current position.

- **Success**: advances the position by `n` and sets `s.view` to the matched span `[start, start+n)`.
- **Failure**: leaves position and view unchanged.

The current position is always `s.view.end`—the byte immediately after the last match.

This primitive enables fixed‑width field extraction without scanning or allocation—ideal for columnar data formats, binary protocols.

###### Example — Extract Host and Port from Fixed‑Width Fields

```c
sno_subject_t s = {0};

sno_bind(&s, "host=alpha");
sno_len(&s, 5);							/* match "host=" */
/* s.view.end now points to "alpha" */
printf("%s\n", s.view.end);

sno_bind(&s, "port=8080");
sno_len(&s, 5);							/* match "host=" */
/* s.view.end now points to "8080" */
printf("%s\n", s.view.end);
```

###### Output:

```
alpha
8080
```

Each `sno_len` consumes a fixed prefix. The position (`s.view.end`) automatically advances to the next field—no manual pointer arithmetic required. The matched prefix is available in `s.view`; the remainder starts at `s.view.end`.

### 2.4`sno_len_var` — Match and Extract in One Step

`sno_len_var(s, n, buf, len)` matches exactly `n` characters and copies them into `buf` with null termination.

- **Success**: advances position by `n` and fills `buf` (requires `len > n` for null terminator).
- **Failure**: leaves position unchanged (atomic rollback on buffer overflow).

###### Example — Extract Port Number "8080"

```c
sno_subject_t s = {0};
char port[5];                    /* 4 digits + null terminator */

sno_bind(&s, "port=8080");
if (sno_len(&s, 5) && sno_len_var(&s, 4, port, sizeof(port)) {
    printf("%i\n", atoi(port));        
}
```

###### Output:

```
8080
```

The `&&` chain composes skipping the prefix with extraction. Because `sno_len_var` is atomic, the entire expression fails cleanly if the buffer is too small—no partial cursor advancement to corrupt subsequent parsing.
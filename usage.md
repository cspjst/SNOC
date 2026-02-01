## 1. The SNO Library

### Implementing the StriNg Oriented symBOlic Language patterns in C

Importing the practical discrete recognition systems from SNOBOL4 into C for powerful string handling and parsing whilst maintaing C idioms.

## 2. Patterns and Pattern Functions

### Patterns Are Functions

Pattern functions are ordinary C functions that match text and advance a cursor. Each consumes zero or more characters from the current position, returns a boolean success/failure result, and leaves the parser in a well-defined state. They compose naturally using C's native `&&` and `||` operators—no macros, no DSL, no hidden engine.

Inspired by SNOBOL4's LEN, SPAN, and BREAK primitives, `sno` adapts their elegance to C's imperative model. Where SNOBOL constructs pattern values then applies them to a subject, `sno` executes matches immediately through direct function calls—eliminating an entire layer of indirection while preserving composability.

The key innovation is the **view**: a half-open span `[begin, end)` that unifies cursor position and extraction. After any successful match, `s.view` *is* the matched substring—no separate assignment step required. The cursor for the next match is simply `s.view.end`. 

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

### 2.4 `sno_var` — Extract Matched Text to Buffer

`sno_var(s, buf, len)` copies the current match into `buf` as a null‑terminated C string. A straightforward way to pull substrings out of a subject string—exactly what most developers reach for when parsing.

- **Success**: `buf` receives a null‑terminated copy of `s.view` - cursor unchanged
- **Failure**: returns `false` if `buf` is too small or arguments are invalid - cursor unchanged

###### Example  — Key Extraction

```c
sno_subject_t s = {0};
char key[16];

sno_bind(&s, "host=alpha");
sno_len(&s, 4);                  /* match "host" */
sno_var(&s, key, sizeof(key));   /* key = "host" */
printf("%s\n", key);             
```

###### Output:

```
host
```

The `sno_var` function is bounds‑checked, such that, if `buf` is too small to hold the match plus a null terminator, it fails safely and leaves the parser state unchanged. This makes it reliable inside `&&` chains—failed copies never corrupt subsequent parsing steps.

Example  — Value Extraction

```c
char val[16];
sno_bind(&s, "host=alpha");
if (sno_len(&s, 4) && sno_lit(&s, '=') && sno_len(&s, 5) && sno_var(&s, val, sizeof(val))) {
    printf("value=%s\n", val);   
}
```

###### Output:

```
value=alpha
```

Use `sno_var` whenever you need a standard C string for `printf`, library calls, or storage. The match happens first (`sno_len`, `sno_span`, etc.), then extraction follows in a single, safe step.

### 2.5`sno_len_var` — Match and Extract in One Step

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
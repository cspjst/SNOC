## 1. The SNO Library

### Implementing the StriNg Oriented symBOlic Language patterns in C

Importing the practical discrete recognition systems from SNOBOL4 into C for powerful string handling and parsing whilst maintaing C idioms.

SNOBOL4 patterns are more powerful than regular expressions (Chomsky Type-3) and can express context-free grammars (Chomsky Type-2) and converting them to C functions unlocks the same pattern matching power. 

A pattern matching that is, IMHO,  more powerful and intuitive to use in C programming than regular expressions.

## 1.1 Design Decisions

sno has no backtracking engine—pattern composition with &&/|| provides explicit, predictable control flow without hidden state unwinding.

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

### 2.2`sno_reset` — Reset All The Things

`sno_reset(s)` returns cursor and capture mark to the start of the subject string.

- **Success**: `s.view` set to empty span `[str.begin, str.begin)` and `s.mark` reset to subject start — returns `true`
- **Failure**: returns `false` if `s` is NULL; otherwise never fails

Enables re-parsing the same subject without rebinding—ideal for multi-pass analysis or retrying patterns from the beginning.

###### Example — Reuse Subject for Multiple Parses

```c
sno_subject_t s = {0};
sno_bind(&s, "host=alpha");
/* First pass: */
...
/* Second pass: Reset and parse from start*/
sno_reset(&s);
...
```

`sno_reset` makes the parser stateless between passes—no need to rebind the subject string or reallocate context. Just reset and go again.

### 2.3`sno_lit` — Single Character Matching

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

### 2.4`sno_len` — Fixed-Length Matching

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

### 2.5 `sno_var` — Extract Matched Text to Buffer

`sno_var(s, buf, len)` copies the current match into `buf` as a null‑terminated C string. A straightforward way to pull substrings out of a subject string—exactly what most developers reach for when parsing.

- **Success**: `buf` receives a null‑terminated copy of `s.view` - cursor unchanged
- **Failure**: returns `false` if `buf` is too small or arguments are invalid - cursor unchanged

After each successful pattern match, `sno_subject_t.view` holds the span matched by that single pattern element—exactly like SNOBOL's ungrouped `.VAR` assignment.

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

### 2.6`sno_len_var` — Match and Extract in One Step

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

### 2.7 `sno_mark` — Place Capture Mark at Current Position

`sno_mark(s)` sets the capture start position to the current cursor (`s.view.end`).

- **Success**: `s.mark` updated to current cursor position — returns `true`
- **Failure**:  returns `false` if `s` is NULL; otherwise never fails (always returns `true` even at end of string)

Marks the start of a multi-element span for later extraction with `sno_cap`. 
**N.B.** The default mark is the subject start (set by `sno_bind`).

###### Example — Mark Identifier Start

```c
sno_subject_t s = {0};
char id[32];

sno_bind(&s, "count=42");							/* default mark at start */
if (sno_any(&s, SNO_LETTERS) && 
    sno_span(&s, SNO_ALNUM_U) &&
    sno_cap(&s, id, sizeof(id)))
{
    printf("identifier=%s\n", id);
}
```

###### Output:

```
identifier=count
```

Without `sno_mark`, extracting multi-element patterns would require manual pointer arithmetic. The mark provides an explicit, debuggable anchor.

### 2.8`sno_cap` — Extract Text Between Mark and Cursor

`sno_cap(s, buf, len)` copies the span `[mark, cursor)` into `buf` with null termination.

- **Success**: `buf` receives null-terminated substring from mark to current cursor — returns `true`
- **Failure**: returns `false` if buffer too small or arguments invalid; cursor and mark unchanged

Captures accumulated matches across multiple pattern elements—exactly what's needed for identifiers, quoted strings, or any multi-token span.

###### Example — Capture Key-Value Pair (In a rather convoluted way to demonstrate.)

```c

sno_subject_t s = {0};
char key[16], val[16];

sno_bind(&s, "host=alpha");
if (sno_span(&s, SNO_ALNUM_U) &&         /* match "host" */
    sno_lit(&s, '=') &&
    sno_mark(&s) &&                      /* mark start of value */
    sno_break(&s, "\r\n") &&             /* match "alpha" */
    sno_cap(&s, val, sizeof(val)) &&     /* capture value */
    sno_reset(&s) &&                     /* reset to start */
    sno_span(&s, SNO_ALNUM_U) &&
    sno_cap(&s, key, sizeof(key)))       /* capture key */
{
    printf("%s\t%s\n", key, val);
}
```

###### Output:

```
host    alpha
```

The `sno_mark` and`sno_cap` pair provide flexible extraction semantics.

### 2.9 `sno_any` — Match Single Character From Set

`sno_any(s, set)` matches exactly one character that appears in `set`.

- **Success**: cursor advances by one; `s.view` becomes the matched character — returns `true`
- **Failure**: cursor and view unchanged (character not in set or end of string) — returns `false`

Essential for precise first-character validation in grammars—such as identifiers that must start with a letter but continue with alphanumerics.

###### Example — Parse Identifiers

```c
#include "sno_constants"		// SNO_LETTERS, SNO_ALNUM_U, and others

sno_subject_t s = {0};
char id[32];

sno_bind(&s, "count=42");
if (sno_any(&s, SNO_LETTERS) &&		
    sno_span(&s, SNO_ALNUM_U) &&
    sno_cap(&s, id, sizeof(id)))
{
    printf("identifier=%s\n", id);
}
```

###### Output:

```
identifier=count
```

Unlike `sno_span`, `sno_any` guarantees exactly one character match—making it the correct choice for "starts with" constraints in identifier grammars.


### 2.10 `sno_notany` — Match Single Character Not In Set

`sno_notany(s, set)` matches exactly one character that does **not** appear in `set`.

-   **Success**: cursor advances by one; `s.view` becomes the matched character — returns `true`
-   **Failure**: cursor and view unchanged (character in set or end of string) — returns `false`

The precise complement of `sno_any`—ideal for skipping delimiters, detecting field boundaries, or matching "anything except" constraints.

###### Example — Skip Non-Identifiers Until Letter
``` C
sno_subject_t s = {0};
char id[32];

sno_bind(&s, "42_count=42");
/* Skip leading non-letters (digits, underscore) */
while (sno_notany(&s, SNO_LETTERS)) {
    /* consumes '4', '2', '_' */
}
/* Now at first letter 'c' */
sno_mark(&s);
if (sno_any(&s, SNO_LETTERS) && 
    sno_span(&s, SNO_ALNUM_U) &&
    sno_cap(&s, id, sizeof(id)))
{
    printf("identifier=%s\n", id);
}
```
###### Output:
```
identifier=count
```
`sno_notany` provides the "negated character class" capability that regex expresses as `[^a-z]`—but with explicit cursor advancement and no hidden scanning behavior. Perfect for controlled tokenization where you need to consume "everything until X" without overshooting.


### 2.11 `sno_tab` — Absolute Cursor Positioning

`sno_tab(s, n)` moves the cursor to absolute offset `n` (0-indexed from start of string).

-   **Success**: cursor advances to offset `n`; `s.view` spans `[old, n)` — returns `true`
-   **Failure**: returns `false` if `n < current position` (leftward move) or `n > length`; cursor unchanged

Positions the cursor at a fixed column—ideal for fixed-width field extraction where field boundaries are known offsets.

###### Example — Extract Fixed-Width Fields
``` C
sno_subject_t s = {0};
char year[5], city[32];

sno_bind(&s, "1290 SEP. 27 CHINA, CHIHLI");
/* Extract year (positions 0-3) */
sno_tab(&s, 4);                          /* cursor → after "1290" */
sno_cap(&s, year, sizeof(year));        /* year = "1290" */

/* Skip to city field (position 14) */
sno_tab(&s, 14);                         /* cursor → after "SEP. 27 " */
sno_mark(&s);
sno_break(&s, ",");                      /* match "CHINA" */
sno_cap(&s, city, sizeof(city));        /* city = "CHINA" */

printf("%s %s\n", year, city);
```
###### Output:
```
1290 CHINA
```
`sno_tab` provides SNOBOL's columnar parsing power—without hidden state or scanning. The matched span is available immediately in `s.view` for zero-copy access or extraction via `sno_cap`.
### 2.12 `sno_rtab` — Position From Right End

`sno_rtab(s, n)` moves the cursor to offset `length - n` (leaving `n` characters at the end).

-   **Success**: cursor advances to `length - n`; `s.view` spans `[old, length-n)` — returns `true`
-   **Failure**: returns `false` if target position < current position; cursor unchanged

Useful for skipping prefixes when the suffix length is known (e.g., "everything except last 4 chars").

###### Example — Skip Prefix, Keep Suffix
``` C
sno_subject_t s = {0};
char suffix[5];

sno_bind(&s, "filename.txt");
sno_rtab(&s, 4);                         /* cursor → before ".txt" */
sno_mark(&s);
sno_rem(&s);                             /* match ".txt" */
sno_cap(&s, suffix, sizeof(suffix));    /* suffix = ".txt" */

printf("extension=%s\n", suffix);
```
###### Output:
```
extension=.txt
```
`sno_rtab(0)` is equivalent to `sno_rem`—match everything to the end.


### 2.11 `sno_rem` — Match Remainder To End

`sno_rem(s)` matches all characters from current cursor to end of string.

-   **Success**: cursor advances to end; `s.view` spans `[old, end)` — returns `true`
-   **Failure**: never fails (always succeeds even with zero-length match)

The simplest way to consume the rest of a line or field—no length calculation required.

###### Example — Parse Key=Value to End of Line
``` C
sno_subject_t s = {0};
char key[16], val[64];

sno_bind(&s, "host=alpha.beta.gamma");
sno_mark(&s);
if (sno_span(&s, SNO_ALNUM_U) &&         /* match "host" */
    sno_lit(&s, '=') &&
    sno_rem(&s) &&                       /* match "alpha.beta.gamma" */
    sno_cap(&s, val, sizeof(val)) &&
    sno_reset(&s) &&
    sno_mark(&s) &&
    sno_span(&s, SNO_ALNUM_U) &&
    sno_cap(&s, key, sizeof(key)))
{
    printf("%s = %s\n", key, val);
}
```
###### Output:
```
host = alpha.beta.gamma
```
`sno_rem` is the endpoint for line-oriented parsing—consume everything that remains without counting characters.


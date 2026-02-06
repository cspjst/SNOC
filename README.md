## 1. The SNO Library

### Implementing the StriNg Oriented symBOlic Language patterns in C

Importing SNOBOL4's discrete recognition systems into C—preserving their power while embracing C's explicit state model. Patterns become ordinary functions composed with native `&&`/`||` operators. No regex complexity. No hidden backtracking. Just pointer arithmetic with compositional elegance.

SNOBOL4 patterns transcend regular expressions (Chomsky Type-3) and express context-free grammars (Type-2). `sno` unlocks this power through deterministic cursor manipulation—zero-copy, O(n) worst-case, and trivially debuggable.

### 1.1 Design Decisions — No Backtracking Engine

SNOBOL4's implicit backtracking scanner rewinds the cursor on failure to explore alternatives—a mechanism that simplifies ambiguous grammars but hides state complexity. `sno` omits backtracking entirely.

Pattern composition uses C's native control flow:

- **Sequencing (`&&`)** — Left operand executes first; success advances cursor for right operand
- **Alternation (`||`)** — Left operand executes first; failure restores cursor before right operand

This yields three observable properties:

1. **State visibility** — Entire parser state = cursor position (`s.view.end`). A debugger shows exactly where matching stopped.
2. **Predictable failure** — Failed alternatives never corrupt cursor position. Next `||` branch retries from original position.
3. **Linear performance** — Each character examined at most once. Catastrophic backtracking (regex `(a+)+b` on long inputs) cannot occur.

Context-free recognition remains possible through explicit recursion—not hidden engine magic:

```c
bool parens(sno_subject_t* s) {
    while (sno_ch(s, '(')) {
        if (!parens(s) || !sno_ch(s, ')')) return false;
    }
    return true;
}
```

The trade-off is deliberate: structure alternatives explicitly, gain deterministic behavior and trivial debugging. This aligns with C's philosophy—visible state, minimal runtime machinery.

### 1.2 Core Model — Subject, View, Cursor



| Concept     | Representation            | Semantics                                                    |
| ----------- | ------------------------- | ------------------------------------------------------------ |
| **Subject** | `s.str` (`[begin, end)`)  | Immutable null-terminated string bound via `sno_bind`        |
| **Cursor**  | `s.view.end`              | Current match position—advances on success, unchanged on failure |
| **View**    | `s.view` (`[begin, end)`) | Half-open span of last match—zero-copy substring             |
| **Mark**    | `s.mark`                  | Explicit anchor for multi-element capture (`sno_mark`/`sno_cap`) |

After any successful match, `s.view` *is* the matched substring—no separate assignment step. Printing requires only pointer arithmetic:

```c
printf("%.*s\n", (int)(s.view.end - s.view.begin), s.view.begin);
```

Extraction to null-terminated strings occurs only when required by legacy APIs (`sno_var`, `sno_cap`). The view serves dual purpose: cursor position for next match *and* immediate substring access—zero allocations during matching.

### 1.3 Composition Idioms

Patterns compose through C's boolean operators with strict semantics:

```c
/* Sequence: match A then B */
sno_lit(&s, "key") && sno_ch(&s, '=') && sno_span(&s, SNO_ALNUM_U)

/* Alternation: match A or B (retry from original position on A failure) */
sno_ch(&s, '"') || sno_ch(&s, '\'')
```

No hidden state machine. Cursor position before `A || B` equals position before `B` if `A` fails. Composition is predictable and debuggable with a single watch expression (`s.view.end`).

### 1.4 Failure Contract

Every pattern function obeys a strict invariant:

> **On failure, cursor (`s.view.end`) and mark (`s.mark`) remain unchanged.**

This enables reliable composition. Alternation chains never leave the cursor in intermediate states:

```c
sno_any(&s, "0123456789") || sno_any(&s, "ABCDEF")
```

If the first `sno_any` fails, the second begins from the *original* position—not a corrupted intermediate state. Extraction functions (`sno_var`, `sno_cap`) also obey this contract: buffer overflow causes immediate failure with no cursor advancement.

## 2. Pattern Primitives

### 2.1 Subject Management

#### 2.1.1 `sno_bind` — Establish Subject String

`sno_bind(s, str)` binds the null‑terminated immutable string `str` to parsing context `s`. The function computes the string length once, caches it in `s->length`, and initializes the cursor to the start of the string with an empty view span `[str, str)`.

- **Success**: `s.str` set to full subject span including null terminator; `s.view` initialized to `[str, str)` (empty span at start); `s.mark` set to subject start; `s.length` cached — returns void
- **Failure**: undefined behavior if `s` or `str` is NULL (caller responsibility)

After binding, all pattern operations (`sno_ch`, `sno_lit`, etc.) operate on this subject until a new string is bound. The subject string must remain valid for the duration of parsing—`sno` never copies or owns the string data.

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

#### 2.1.2 `sno_reset` — Return to Start

`sno_reset(s)` returns cursor and capture mark to the start of the subject string.

- **Success**: `s.view` set to empty span `[str.begin, str.begin)` and `s.mark` reset to subject start — returns `true`
- **Failure**: returns `false` if `s` is NULL; otherwise never fails

Enables re-parsing the same subject without rebinding—ideal for multi-pass analysis or retrying patterns from the beginning.

###### Example — Reuse Subject for Multiple Parses

```c
sno_subject_t s = {0};
sno_bind(&s, "host=alpha");
/* First pass: extract key */
sno_mark(&s);
sno_span(&s, SNO_ALNUM_U);
char key[16]; sno_cap(&s, key, sizeof(key));

/* Second pass: reset and extract value */
sno_reset(&s);
sno_span(&s, SNO_ALNUM_U);
sno_ch(&s, '=');
sno_mark(&s);
sno_break(&s, "\r\n");
char val[16]; sno_cap(&s, val, sizeof(val));

printf("%s=%s\n", key, val);
```

###### Output:

```
host=alpha
```

`sno_reset` makes the parser stateless between passes—no need to rebind the subject string or reallocate context. Just reset and go again.

### 2.2 Literals

#### 2.2.1 `sno_ch` — Match Single Character

`sno_ch(s, ch)` matches exactly one character `ch` at the current cursor position in subject string `s`.

- **Success**: the cursor advances by one position and `s->view` becomes a one‑character span `[old_cursor, old_cursor+1)` containing the matched character. The function returns `true`.
- **Failure**: both cursor and view remain unchanged. The function returns `false`.

This primitive is the building block for delimiter recognition, comment detection, and simple token parsing—all without allocations or hidden state.

###### Example — Detect Shebang Lines (`#!`)

```c
sno_subject_t s = {0};
sno_bind(&s, "#!/bin/sh");

if (sno_ch(&s, '#') && sno_ch(&s, '!')) {
    printf("Shebang interpreter: %s\n", s.view.end); 
}
```

###### Output:

```
Shebang interpreter: /bin/sh
```

#### 2.2.2 `sno_lit` — Match Literal String

`sno_lit(s, lit)` matches the exact null‑terminated character sequence `lit` starting at the current cursor position.

- **Success**: cursor advances by `strlen(lit)` characters; `s->view` spans the matched literal sequence — returns `true`
- **Failure**: cursor and view remain unchanged if any character mismatches or end-of-string encountered prematurely — returns `false`

Ideal for keyword matching and multi-character token recognition where exact spelling matters.

###### Example — Command Dispatch with Full Keywords

```c
sno_subject_t s = {0};
sno_bind(&s, "load program.dope");

if (sno_lit(&s, "load") && sno_ch(&s, ' ')) {
    printf("Loading: %s\n", s.view.end);  /* remainder after "load " */
}
```

###### Output:

```
Loading: program.dope
```

Unlike regex, `sno_lit` performs exact byte-for-byte comparison with no hidden escaping rules—what you write is what you match.

### 2.3 Length

#### 2.3.1 `sno_len` — Fixed-Length Matching

`sno_len(s, n)` matches exactly `n` characters starting from the current cursor position.

- **Success**: cursor advances by `n` positions; `s->view` spans `[old_cursor, old_cursor+n)` — returns `true`
- **Failure**: cursor and view remain unchanged if fewer than `n` characters remain before null terminator — returns `false`

Enables fixed‑width field extraction without scanning or allocation—ideal for columnar data formats and binary protocols where field boundaries are known offsets.

###### Example — Extract Fixed-Width Fields from Historical Records

```c
sno_subject_t s = {0};
char year[5], month[6];

sno_bind(&s, "1290 SEP. 27 CHINA, CHIHLI");

/* Year occupies columns 0-3 (4 characters) */
sno_len(&s, 4);
sno_var(&s, year, sizeof(year));        /* year = "1290" */

/* Skip space and extract month (columns 5-8) */
sno_ch(&s, ' ');
sno_len(&s, 4);
sno_var(&s, month, sizeof(month));      /* month = "SEP." */

printf("%s %s\n", year, month);
```

###### Output:

```
1290 SEP.
```

Each `sno_len` consumes a precise prefix. The position (`s.view.end`) automatically advances to the next field—no manual pointer arithmetic required. The matched prefix is available immediately in `s.view` for zero-copy access or extraction via `sno_var`.

### 2.4 Character Sets

#### 2.4.1 `sno_any` — Match Single Character From Set

`sno_any(s, set)` matches exactly one character that appears in the null‑terminated character set `set`.

- **Success**: cursor advances by one position; `s->view` becomes the matched character — returns `true`
- **Failure**: cursor and view unchanged if current character not in `set` or at end-of-string — returns `false`

Essential for precise first-character validation in grammars—such as identifiers that must start with a letter but continue with alphanumerics.

###### Example — Parse Identifiers (Letter Followed by Alphanumerics)

```c
sno_subject_t s = {0};
char id[32];

sno_bind(&s, "count=42");
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

Unlike `sno_span`, `sno_any` guarantees exactly one character match—making it the correct choice for "starts with" constraints in identifier grammars.

#### 2.4.2 `sno_notany` — Match Single Character Not In Set

`sno_notany(s, set)` matches exactly one character that does **not** appear in the null‑terminated character set `set`.

- **Success**: cursor advances by one position; `s->view` becomes the matched character — returns `true`
- **Failure**: cursor and view unchanged if current character *is* in `set` or at end-of-string — returns `false`

The precise complement of `sno_any`—ideal for skipping delimiters, detecting field boundaries, or matching "anything except" constraints.

###### Example — Skip Non-Identifiers Until Letter

```c
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

#### 2.4.3 `sno_span` — Match One or More Characters From Set

`sno_span(s, set)` consumes the longest possible prefix of characters **all** belonging to the null‑terminated character set `set`.

- **Success**: cursor advances over one or more matching characters; `s->view` spans the entire matched sequence — returns `true`
- **Failure**: cursor and view unchanged if current character not in `set` (requires ≥1 match) — returns `false`

The workhorse for token extraction—identifiers, numbers, whitespace runs—where you want to consume as much as possible from a character class.

###### Example — Extract Numeric Value After Equals Sign

```c
sno_subject_t s = {0};
char num[16];

sno_bind(&s, "port=8080");
sno_lit(&s, "port");
sno_ch(&s, '=');
sno_mark(&s);
if (sno_span(&s, SNO_DIGITS) && sno_cap(&s, num, sizeof(num))) {
    printf("port number: %s\n", num);
}
```

###### Output:

```
port number: 8080
```

`sno_span` stops at the first character *not* in the set—guaranteeing maximal consumption without lookahead ambiguity.

#### 2.4.4 `sno_break` — Match Zero or More Characters Until Set

`sno_break(s, set)` consumes the longest possible prefix of characters **not** belonging to the null‑terminated character set `set`.

- **Success**: cursor advances over zero or more non-matching characters; `s->view` spans the consumed sequence — returns `true` (even for zero-length match)
- **Failure**: only fails on NULL arguments; never fails due to input content

The complement of `sno_span`—ideal for extracting content *until* a delimiter without consuming the delimiter itself.

###### Example — Extract Value Before Newline

```c
sno_subject_t s = {0};
char val[64];

sno_bind(&s, "host=alpha.beta.gamma\r\n");
sno_lit(&s, "host");
sno_ch(&s, '=');
sno_mark(&s);
if (sno_break(&s, "\r\n") && sno_cap(&s, val, sizeof(val))) {
    printf("value: %s\n", val);
}
```

###### Output:

```
value: alpha.beta.gamma
```

Unlike `sno_span`, `sno_break` succeeds with an empty match when the cursor starts at a delimiter character—making it safe for optional content extraction.

### 2.5 Positioning

#### 2.5.1 `sno_tab` — Absolute Cursor Positioning

`sno_tab(s, n)` moves the cursor to absolute offset `n` (0-indexed from start of subject string).

- **Success**: cursor advances to offset `n`; `s->view` spans `[old_cursor, n)` — returns `true`
- **Failure**: returns `false` if `n < current position` (leftward move disallowed) or `n > length`; cursor unchanged

Positions the cursor at a fixed column—ideal for fixed-width field extraction where field boundaries are known offsets.

###### Example — Extract Fixed-Width Fields from Historical Records

```c
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

#### 2.5.2 `sno_rtab` — Position From Right End

`sno_rtab(s, n)` moves the cursor to offset `length - n` (leaving exactly `n` characters at the end of the subject).

- **Success**: cursor advances to `length - n`; `s->view` spans `[old_cursor, length-n)` — returns `true`
- **Failure**: returns `false` if target position < current position (leftward move disallowed); cursor unchanged

Useful for skipping prefixes when the suffix length is known (e.g., "everything except last 4 chars").

###### Example — Extract File Extension

```c
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

#### 2.5.3 `sno_rem` — Match Remainder To End

`sno_rem(s)` matches all characters from current cursor position to the end of the subject string (before null terminator).

- **Success**: cursor advances to end; `s->view` spans `[old_cursor, end)` — returns `true`
- **Failure**: never fails (always succeeds even with zero-length match at end)

The simplest way to consume the rest of a line or field—no length calculation required.

###### Example — Parse Key=Value to End of Line

```c
sno_subject_t s = {0};
char key[16], val[64];

sno_bind(&s, "host=alpha.beta.gamma");
sno_mark(&s);
if (sno_span(&s, SNO_ALNUM_U) &&         /* match "host" */
    sno_ch(&s, '=') &&
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

### 2.6 Capture

#### 2.6.1 `sno_mark` — Place Capture Mark at Current Position

`sno_mark(s)` sets the capture start position to the current cursor (`s.view.end`).

- **Success**: `s.mark` updated to current cursor position — returns `true`
- **Failure**: returns `false` if `s` is NULL; otherwise never fails (always returns `true` even at end of string)

Marks the start of a multi-element span for later extraction with `sno_cap`. The default mark is the subject start (set by `sno_bind`).

###### Example — Mark Identifier Start

```c
sno_subject_t s = {0};
char id[32];

sno_bind(&s, "count=42");                /* default mark at start */
sno_mark(&s);                            /* explicitly mark start (redundant here) */
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

#### 2.6.2 `sno_cap` — Extract Text Between Mark and Cursor

`sno_cap(s, buf, len)` copies the span `[mark, cursor)` into `buf` with null termination.

- **Success**: `buf` receives null-terminated substring from mark to current cursor — returns `true`
- **Failure**: returns `false` if buffer too small or arguments invalid; cursor and mark unchanged

Captures accumulated matches across multiple pattern elements—exactly what's needed for identifiers, quoted strings, or any multi-token span.

###### Example — Capture Key-Value Pair

```c
sno_subject_t s = {0};
char key[16], val[16];

sno_bind(&s, "host=alpha");
sno_mark(&s);                            /* mark start of key */
if (sno_span(&s, SNO_ALNUM_U) &&         /* match "host" */
    sno_cap(&s, key, sizeof(key)) &&     /* extract key */
    sno_ch(&s, '=') &&
    sno_mark(&s) &&                      /* mark start of value */
    sno_break(&s, "\r\n") &&             /* match "alpha" */
    sno_cap(&s, val, sizeof(val)))       /* extract value */
{
    printf("%s\t%s\n", key, val);
}
```

###### Output:

```
host	alpha
```

The `sno_mark` and `sno_cap` pair provide flexible extraction semantics without hidden state—exactly matching SNOBOL's `.VAR` assignment but with explicit boundaries.

#### 2.6.3 `sno_var` — Extract Current Match Span

`sno_var(s, buf, len)` copies the current match span (`s.view`) into `buf` as a null‑terminated C string.

- **Success**: `buf` receives null-terminated copy of `s.view` — cursor unchanged — returns `true`
- **Failure**: returns `false` if `buf` too small to hold match plus null terminator or arguments invalid — cursor unchanged

A straightforward way to pull substrings out of a subject string—exactly what most developers reach for when parsing. Unlike `sno_cap`, it extracts only the *last* matched element rather than a multi-element span.

###### Example — Key Extraction

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

The `sno_var` function is bounds‑checked— if `buf` is too small to hold the match plus a null terminator, it fails safely and leaves the parser state unchanged. This makes it reliable inside `&&` chains—failed copies never corrupt subsequent parsing steps.

### 2.7 Balanced Delimiters

#### 2.7.1 `sno_bal` — Match Balanced Expressions

`sno_bal(s, open, close)` matches a nonnull string balanced with respect to delimiter pair `(open, close)`.

- **Success**: cursor advances over entire balanced expression **including** outer delimiters; `s.view` spans full expression — returns `true`
- **Failure**: cursor unchanged; returns `false` if opening delimiter absent or expression unbalanced (mismatched/unclosed delimiters)

Validates nesting deterministically through explicit recursion—no backtracking required. Matches exactly one balanced expression (adjacent pairs require repeated calls).

N.B. `sno_bal(s, open, close)` generalizes SNOBOL's hardcoded `()` to arbitrary delimiter pairs while preserving deterministic semantics.

> **Warning**: Pathological inputs (`(((...)))` with 10⁶ nesting) may exhaust stack. Typical parsers rarely exceed hundreds of nesting levels.

###### Example — Parse Nested Expression Interior

```c
sno_subject_t s = {0};
char expr[64];

sno_bind(&s, "A(B(C)D)E");
sno_mark(&s);
if (sno_ch(&s, 'A') &&
    sno_bal(&s, '(', ')') &&          /* matches "(B(C)D)" */
    sno_ch(&s, 'E') &&
    sno_cap(&s, expr, sizeof(expr)))
{
    printf("inner=%s\n", expr);       /* → inner=(B(C)D) */
}
```

###### Output:

```
inner=(B(C)D)
```

Demonstrates that context‑free recognition does not require backtracking. The recursive descent algorithm:

1. Advances cursor deterministically
2. Maintains visible state (`s.view.end`)
3. Fails fast on imbalance (no hidden stack unwinding)

### 2.8 Position Predicates

#### 2.8.1 `sno_at` / `sno_at_r` — Cursor Position Tests

`sno_at(s, n)` and `sno_at_r(s, n)` test cursor position without advancing state.

- **`sno_at`**: succeeds if cursor is at absolute offset `n` (0-indexed from start)
- **`sno_at_r`**: succeeds if cursor is at offset `length - n` (leaving `n` characters at end)
- **Never move cursor** — pure predicates for validation/assertion
- **Return boolean** — composable with `&&` after pattern matches

Use these to enforce column constraints or validate parse boundaries—exactly like SNOBOL's PCS/RPOS, but expressed idiomatically in C.

###### Example — Fixed-Width Field Validation

```c
sno_subject_t s = {0};
char year[5], month[6];

sno_bind(&s, "1290 SEP. 27 CHINA, CHIHLI");

/* Year must occupy columns 0-3 */
sno_mark(&s);
if (sno_len(&s, 4) && sno_at(&s, 4) && sno_cap(&s, year, sizeof(year))) {
    printf("year=%s\n", year);          /* → year=1290 */
}

/* Skip to month field (columns 5-8) */
sno_tab(&s, 5);
sno_mark(&s);
if (sno_len(&s, 4) && sno_at(&s, 9) && sno_cap(&s, month, sizeof(month))) {
    printf("month=%s\n", month);        /* → month=SEP. */
}
```

###### Output:

```
year=1290
month=SEP.
```

The `sno_at` checks act as **guard rails**—if a field overruns its column boundary, the entire pattern fails cleanly.

###### Example — Assert End of Line

```c
sno_subject_t s = {0};
char year[5], month[6];

sno_bind(&s, "1290 SEP. 27 CHINA, CHIHLI");

/* Year must occupy columns 0-3 */
sno_mark(&s);
if (sno_len(&s, 4) && sno_at(&s, 4) && sno_cap(&s, year, sizeof(year))) {
    printf("year=%s\n", year);          /* → year=1290 */
}

/* Skip to month field (columns 5-8) */
sno_tab(&s, 5);
sno_mark(&s);
if (sno_len(&s, 4) && sno_at(&s, 9) && sno_cap(&s, month, sizeof(month))) {
    printf("month=%s\n", month);        /* → month=SEP. */
}
```

`sno_at_r(s, 0)` is the way to assert "consumed the entire line"—critical for line-oriented parsers that must reject trailing garbage.

## 3. String Utilities (Chapter 3)

*To be continued: `sno_str_equal`, `sno_str_compare`, `sno_view_size`, `sno_str_trim`, `sno_str_replace`, `sno_str_dupl`, `sno_str_cat2`—zero-copy utilities that complement pattern primitives for complete text processing.*

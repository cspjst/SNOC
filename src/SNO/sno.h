/**
 * @file sno.h
 * @brief SNOBOL-inspired zero-copy pattern matching for C
 *
 * A minimal, composable pattern matching toolkit inspired by SNOBOL4's
 * elegant string algebra—without regex complexity or hidden state.
 *
 * @section why Why This Beats Regex for Parsing
 *
 * Regex conflates pattern syntax, engine state, and extraction semantics
 * into a write-only DSL prone to backtracking explosions and opaque failures.
 * This toolkit embraces a simpler truth: <em>parsing is cursor manipulation</em>.
 *
 * Key advantages:
 * - <b>Zero-copy views</b>: Match spans are [begin,end) pointers into the
 *   original immutable string—no allocations during matching.
 * - <b>Composable primitives</b>: Patterns are functions composed with C's
 *   native &&/|| operators—no string interpolation or escaping hell.
 * - <b>Fail-fast semantics</b>: No backtracking surprises—O(n) worst-case
 *   performance with explicit cursor state visible in a debugger.
 * - <b>Atomic extraction</b>: sno_len_var() rolls back cursor on buffer
 *   overflow—enabling safe alternation (pattern || fallback).
 * - <b>No hidden state</b>: Entire parser state = cursor position. Reset with
 *   one call. Thread-safe by design (immutable subject strings).
 *
 * @section model Core Model
 *
 * - <b>Subject</b>: Immutable null-terminated string (sno_bind)
 * - <b>Cursor</b>: Current position = s->view.end
 * - <b>Pattern</b>: Function that advances cursor on success, leaves unchanged on failure
 * - <b>View</b>: Half-open span [begin,end) capturing matched substring
 * - <b>Composition</b>: Sequence = &&, Alternation = || (native C operators)
 *
 * @section snobol SNOBOL Heritage, C Pragmatism
 *
 * Faithfully implements SNOBOL's SPAN/BREAK/LEN primitives with their
 * precise semantics—but drops SNOBOL's scanning behavior in favor of
 * fail-fast parsing (more predictable for structured data).
 *
 * No interpreter. No bytecode. No GC. Just pointer arithmetic composed
 * with C's native control flow—giving you SNOBOL's expressiveness with
 * C's performance and debuggability.
 *
 * @section example Example: Parse "key=value"
 * @code{.c}
 * sno_subject_t s;
 * sno_bind(&s, "host=alpha");
 *
 * // SPAN(letters) → key
 * if (sno_span(&s, "abcdefghijklmnopqrstuvwxyz")) {
 *     sno_view_t key = s.view;
 *     if (sno_lit(&s, '=') && sno_break(&s, "\r\n")) {
 *         sno_view_t val = s.view;
 *         // Use key/val spans directly—zero copies
 *         printf("KEY='%.*s' VAL='%.*s'\n",
 *                (int)(key.end-key.begin), key.begin,
 *                (int)(val.end-val.begin), val.begin);
 *     }
 * }
 * @endcode
 *
 * @author Inspired by SNOBOL4 (Griswold et al., Bell Labs 1962–1967)
 * @license Public domain / MIT—use freely in any project
 */
#ifndef SNO_H
#define SNO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/** Immutable character type alias for const-correct string handling */
typedef const char cstr_t;

/**
 * @brief Half-open string slice [begin, end)
 *
 * Represents a substring view into an immutable subject string.
 * Zero-copy: no allocation, no ownership—just pointer range.
 * Length = end - begin (bytes). Empty when begin == end.
 */
typedef struct {
    cstr_t* begin;  /**< Inclusive start of slice */
    cstr_t* end;    /**< Exclusive end of slice (one past last char) */
} sno_view_t;

/**
 * @brief Parsing context for SNOBOL-style pattern matching
 *
 * Maintains subject string state and current match position.
 * All pattern primitives advance s->view.end on success; leave unchanged on failure.
 */
typedef struct {
    sno_view_t str;    /**< Full subject string [begin, end) including null terminator */
    sno_view_t view;   /**< Current match span [begin, end); cursor = view.end */
    size_t length;     /**< Cached strlen (excluding null terminator) */
} sno_subject_t;

/**
 * @brief Bind subject string to parsing context
 *
 * Computes strlen once and caches result. Initializes cursor to start of string.
 * @param s Parsing context (must not be NULL)
 * @param c Null-terminated immutable string (must not be NULL)
 * @note After binding: s->view = [c, c), cursor at start
 */
void sno_bind(sno_subject_t* s, cstr_t* c);

/**
 * @brief Reset cursor to start of subject string
 *
 * Sets s->view to empty span at beginning: [str.begin, str.begin)
 * @param s Parsing context (must not be NULL)
 */
void sno_reset(sno_subject_t* s);

/**
 * @brief Match single literal character at cursor
 *
 * Succeeds iff *cursor == ch. Advances cursor by 1 on success.
 * @param s Parsing context (must not be NULL)
 * @param ch Character to match
 * @return true if match succeeds; false otherwise (cursor unchanged on failure)
 */
bool sno_lit(sno_subject_t* s, char ch);

/**
 * @brief Match exactly n characters from cursor
 *
 * Succeeds iff n characters remain before null terminator.
 * @param s Parsing context (must not be NULL)
 * @param n Exact number of characters to consume
 * @return true if match succeeds; false otherwise (cursor unchanged on failure)
 * @note Fails if n == 0 or cursor + n >= str.end
 */
bool sno_len(sno_subject_t* s, size_t n);

/**
 * @brief Match 1+ characters from set (SNOBOL SPAN primitive)
 *
 * Consumes longest prefix of characters ALL in 'set'.
 * @param s Parsing context (must not be NULL)
 * @param set Null-terminated string of allowed characters (must not be NULL)
 * @return true if ≥1 character matched; false otherwise (cursor unchanged on failure)
 * @note Requires at least one match (unlike BREAK). Stops at first char not in set.
 */
bool sno_span(sno_subject_t* s, const char* set);

/**
 * @brief Match 0+ characters until set member (SNOBOL BREAK primitive)
 *
 * Consumes longest prefix of characters NOT in 'set'.
 * @param s Parsing context (must not be NULL)
 * @param set Null-terminated string of break characters (must not be NULL)
 * @return true always (even for zero-length match); false only on NULL args
 * @note Succeeds with empty match when cursor starts at set member.
 *       Stops at first char in set (without consuming it).
 */
bool sno_break(sno_subject_t* s, const char* set);

/**
 * @brief Extract current match span into buffer (zero-copy view → copy)
 *
 * Copies s->view span [begin, end) into buf with null termination.
 * @param s Parsing context (must not be NULL)
 * @param buf Destination buffer (must not be NULL)
 * @param buflen Size of buf in bytes (must be > 0)
 * @return true if copy succeeds (len < buflen); false on buffer overflow or NULL args
 * @note Requires buflen > (end - begin) to accommodate null terminator.
 *       Does not modify cursor position.
 */
bool sno_var(sno_subject_t* s, char* buf, size_t buflen);

/**
 * @brief Atomic LEN(n) + extraction (SNOBOL LEN(n) . VAR idiom)
 *
 * Matches n characters AND extracts to buffer as single transaction.
 * @param s Parsing context (must not be NULL)
 * @param n Number of characters to match
 * @param buf Destination buffer (must not be NULL)
 * @param buflen Size of buf in bytes (must be > 0)
 * @return true if both match and extraction succeed; false otherwise
 * @note Atomic semantics: cursor unchanged if extraction fails (rollback).
 *       Enables safe composition: sno_len_var(...) || alternative_pattern()
 */
bool sno_len_var(sno_subject_t* s, size_t n, char* buf, size_t buflen);

/**
 * @brief Match 0+ whitespace characters (space, tab, CR, LF)
 *
 * Always succeeds (even with zero-length match). Advances cursor over
 * consecutive whitespace characters if present; otherwise leaves cursor
 * unchanged and sets view to empty span [cursor, cursor).
 * @param s Parsing context (must not be NULL)
 * @return true always (cursor advanced 0+ positions)
 * @note Whitespace set = " \t\r\n"
 * @note Zero-length success: view = [cursor, cursor) when no whitespace present
 */
 bool sno_ws(sno_subject_t* s);

/**
 * @brief Match 1+ whitespace characters
 *
 * Requires at least one whitespace character. Fails at non-whitespace.
 * @param s Parsing context (must not be NULL)
 * @return true if ≥1 whitespace matched; false otherwise (cursor unchanged)
 * @note Whitespace set = " \t\r\n"
 */
static inline bool sno_ws1(sno_subject_t* s) {
    return sno_span(s, " \t\r\n");
}

/**
 * @brief Match 1+ ASCII digits (0-9)
 *
 * SNOBOL idiom: SPAN('0123456789')
 * @param s Parsing context (must not be NULL)
 * @return true if ≥1 digit matched; false otherwise (cursor unchanged)
 */
static inline bool sno_digits(sno_subject_t* s) {
    return sno_span(s, "0123456789");
}

/**
 * @brief Match 1+ ASCII letters (A-Z, a-z)
 *
 * SNOBOL idiom: SPAN('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz')
 * @param s Parsing context (must not be NULL)
 * @return true if ≥1 letter matched; false otherwise (cursor unchanged)
 */
static inline bool sno_alpha(sno_subject_t* s) {
    return sno_span(s, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
}

/**
 * @brief Match 1+ alphanumeric characters (A-Z, a-z, 0-9, underscore)
 *
 * Includes underscore for identifier compatibility.
 * @param s Parsing context (must not be NULL)
 * @return true if ≥1 alnum char matched; false otherwise (cursor unchanged)
 */
static inline bool sno_alnum(sno_subject_t* s) {
    return sno_span(s, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
}

#endif


/**
 * @brief Match everything until (but not including) delimiter character
 *
 * SNOBOL idiom: BREAK(delimiter)
 * @param s Parsing context (must not be NULL)
 * @param delim Single-character delimiter (e.g., ',', '=', '\n')
 * @return true always (even for empty match); false only on NULL args
 * @note Does not consume the delimiter—cursor stops before it.
 * @note Empty match succeeds when cursor starts at delimiter.
 */
 /*
static inline bool sno_until(sno_subject_t* s, char delim) {
    char set[2] = {' ', '\0'};
    set[0] = delim;
    return sno_break(s, set);
}
*/

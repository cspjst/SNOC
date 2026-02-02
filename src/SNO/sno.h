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
    cstr_t* mark;      /**< capture start position */
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
 * @brief Reset cursor and mark to start of subject string
 *
 * Sets s->view to empty span [str.begin, str.begin) and s->mark to str.begin.
 * @param s Parsing context (must not be NULL)
 * @return true if reset succeeded; false if s is NULL
 */
bool sno_reset(sno_subject_t* s);
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
 * @brief Place capture mark at current cursor position
 *
 * Sets the capture start to the current cursor (s.view.end).
 * @param s Parsing context (must not be NULL)
 * @return true always (even at end of string)
 * @note Default mark is start of subject (set by sno_bind/sno_reset)
 */
bool sno_mark(sno_subject_t* s);

/**
 * @brief Extract text between mark and current cursor
 *
 * Copies the span [mark, cursor) into buf with null termination.
 * @param s Parsing context (must not be NULL)
 * @param buf Destination buffer (must not be NULL)
 * @param buflen Size of buf in bytes (must be > 0)
 * @return true if copy succeeds (span length < buflen); false on overflow or NULL args
 * @note Requires prior sno_mark() or relies on default mark=start of subject
 */
bool sno_cap(sno_subject_t* s, char* buf, size_t buflen);

/**
 * @brief Match single character from set (SNOBOL ANY primitive)
 *
 * Matches exactly one character that appears in 'set'.
 * @param s Parsing context (must not be NULL)
 * @param set Null-terminated string of allowed characters (must not be NULL)
 * @return true if character matched; false otherwise (cursor unchanged on failure)
 * @note Fails at end of string or when current character not in set.
 */
bool sno_any(sno_subject_t* s, const char* set);



#endif

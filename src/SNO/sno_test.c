/* sno_test.c */
#include "sno.h"
#include <assert.h>
#include <string.h>

void sno_test(void)
{
    sno_subject_t s = {0};
    char buf[64];

    /* sno_bind */
    sno_bind(&s, "hello");
    assert(s.str.begin != NULL);
    assert(s.str.end != NULL);
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);
    assert(s.length == 5);

    /* sno_reset */
    sno_bind(&s, "abcdef");
    assert(sno_len(&s, 3));
    assert(s.view.end - s.str.begin == 3);
    sno_reset(&s);
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);

    /* sno_lit success */
    sno_bind(&s, "xyz");
    assert(sno_lit(&s, 'x'));
    assert(s.view.end == s.str.begin + 1);
    assert(sno_lit(&s, 'y'));
    assert(s.view.end == s.str.begin + 2);

    /* sno_lit failure (cursor unchanged) */
    sno_bind(&s, "abc");
    assert(!sno_lit(&s, 'x'));
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);
    assert(sno_lit(&s, 'a'));
    assert(!sno_lit(&s, 'x'));
    assert(s.view.begin == s.str.begin);   // unchanged on failure
    assert(s.view.end == s.str.begin + 1);

    /* sno_len success */
    sno_bind(&s, "12345");
    assert(sno_len(&s, 3));
    assert(s.view.end == s.str.begin + 3);
    assert(sno_len(&s, 2));
    assert(s.view.end == s.str.begin + 5);

    /* sno_len failure (cursor unchanged) */
    sno_bind(&s, "short");
    assert(!sno_len(&s, 10));
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);
    assert(sno_len(&s, 3));
    assert(!sno_len(&s, 3));
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin + 3);

    /* sno_span success (≥1 char) */
    sno_bind(&s, "123abc");
    assert(sno_span(&s, "0123456789"));
    assert(s.view.end - s.view.begin == 3);  // "123"
    assert(sno_span(&s, "abc"));
    assert(s.view.end - s.view.begin == 3);  // "abc"

    /* sno_span failure (empty match → fail) */
    sno_bind(&s, "abc");
    assert(!sno_span(&s, "0123456789"));
    assert(s.view.begin == s.view.end);      // cursor unchanged

    /* sno_break success (0+ chars) — non-empty */
    sno_bind(&s, "abc def");
    assert(sno_break(&s, " "));
    assert(s.view.end - s.view.begin == 3);  // "abc"

    /* sno_break success (0+ chars) — EMPTY match at break char */
    sno_bind(&s, " next");                   // space at start
    assert(sno_break(&s, " "));
    assert(s.view.begin == s.view.end);      // zero-length match

    /* sno_break success — entire remainder */
    sno_bind(&s, "nospaces");
    assert(sno_break(&s, " "));
    assert(s.view.end == s.str.end - 1);     // matched all (excluding '\0')

    /* sno_break stops at member */
    sno_bind(&s, "hello,world");
    assert(sno_break(&s, ","));
    assert(s.view.end - s.view.begin == 5);  // "hello"

    /* sno_var success */
    sno_bind(&s, "copyme");
    assert(sno_len(&s, 4));
    assert(sno_var(&s, buf, sizeof(buf)));
    assert(memcmp(buf, "copy", 4) == 0);
    assert(buf[4] == '\0');

    /* sno_var failure (buffer too small) */
    sno_bind(&s, "toolong");
    assert(sno_len(&s, 7));
    assert(!sno_var(&s, buf, 5));            // needs 8 bytes (7+null)
    assert(!sno_var(&s, NULL, 10));
    assert(!sno_var(&s, buf, 0));

    /* sno_len_var success */
    sno_bind(&s, "extract");
    assert(sno_len_var(&s, 4, buf, sizeof(buf)));
    assert(memcmp(buf, "extr", 4) == 0);
    assert(buf[4] == '\0');

    /* sno_len_var failure (len fails) */
    sno_bind(&s, "short");
    assert(!sno_len_var(&s, 10, buf, sizeof(buf)));
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);

    /* sno_len_var failure (var fails) */
    sno_bind(&s, "longenough");
    assert(!sno_len_var(&s, 5, buf, 3));     // buf too small
    assert(s.view.begin == s.str.begin);
    assert(s.view.end == s.str.begin);       // atomic failure

    /* Composition test */
    sno_bind(&s, "key=value");
    assert(sno_span(&s, "abcdefghijklmnopqrstuvwxyz"));
    assert(s.view.end - s.view.begin == 3);  // "key"
    assert(sno_lit(&s, '='));
    assert(sno_break(&s, "\r\n"));
    assert(s.view.end - s.view.begin == 5);  // "value"

    /* Final sanity: full parse */
    sno_bind(&s, "1234 SEP 27");
    assert(sno_len(&s, 4));
    assert(sno_lit(&s, ' '));
    assert(sno_span(&s, "ABCDEFGHIJKLMNOPQRSTUVWXYZ."));
    assert(sno_lit(&s, ' '));
    assert(sno_len(&s, 2));
    assert(s.view.end == s.str.end - 1);     // consumed all (minus '\0')

    /* === sno_ws / sno_ws1 === */
    sno_bind(&s, "   text");
    assert(sno_ws(&s));                      // matches "   "
    assert(s.view.end - s.view.begin == 3);  // 3 spaces consumed
    assert(sno_ws(&s));                      // empty match at 't' (still succeeds)
    assert(s.view.begin == s.view.end);      // zero-length span at cursor

    sno_bind(&s, "text");
    assert(sno_ws(&s));                      // empty match at 't' (succeeds)
    assert(s.view.begin == s.view.end);      // zero-length span
    assert(!sno_ws1(&s));                    // requires ≥1 whitespace → fails
    assert(s.view.begin == s.view.end);      // cursor unchanged on failure

    sno_bind(&s, "  text");
    assert(sno_ws1(&s));                     // matches "  "
    assert(s.view.end - s.view.begin == 2);

    /* === sno_digits === */
    sno_bind(&s, "123abc");
    assert(sno_digits(&s));
    assert(s.view.end - s.view.begin == 3);  // "123"

    assert(!sno_digits(&s));                 // fails at 'a'
    assert(s.view.end - s.view.begin == 3);  // view UNCHANGED (still "123")
    assert(s.view.end == s.str.begin + 3);   // cursor unchanged

    /* === sno_alpha === */
    sno_bind(&s, "abc123");
    assert(sno_alpha(&s));
    assert(s.view.end - s.view.begin == 3);  // "abc"

    /* === sno_alnum === */
    sno_bind(&s, "a1b2c3!");
    assert(sno_alnum(&s));
    assert(s.view.end - s.view.begin == 6);  // "a1b2c3"

    /* === sno_until === */
    sno_bind(&s, "key=value");
    assert(sno_until(&s, '='));
    assert(s.view.end - s.view.begin == 3);  // "key"
    assert(sno_lit(&s, '='));                // cursor stopped BEFORE '='

    sno_bind(&s, "=value");                  // empty match at delimiter
    assert(sno_until(&s, '='));
    assert(s.view.begin == s.view.end);      // zero-length span
    assert(sno_lit(&s, '='));                // consumes the '='
}

/* sno_test.c */
#include "sno.h"
#include "sno_constants.h"
#include <assert.h>
#include <string.h>

void sno_test(void) {
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

    /* === sno_any === */
    sno_bind(&s, "alpha");
    assert(sno_any(&s, SNO_LETTERS));      // 'a' in letters
    assert(s.view.end - s.view.begin == 1);
    assert(sno_any(&s, SNO_LETTERS));      // 'l' in letters
    assert(!sno_any(&s, SNO_DIGITS));      // 'p' not in digits → fail
    assert(s.view.end == s.str.begin + 2); // cursor unchanged after failure

    sno_bind(&s, "42");
    assert(!sno_any(&s, SNO_LETTERS));     // '4' not in letters → fail
    assert(s.view.begin == s.view.end);    // empty view (cursor at start)

    sno_bind(&s, "");
    assert(!sno_any(&s, SNO_LETTERS));     // empty string → fail

    /* sno_mark / sno_cap */

    /* Test 1: Default mark = start of subject after sno_bind */
    sno_bind(&s, "hello");
    assert(s.mark == s.str.begin);          /* mark at start */
    assert(sno_cap(&s, buf, sizeof(buf)));  /* capture empty span */
    assert(strcmp(buf, "") == 0);           /* → "" */

    /* Test 2: Mark at start, capture after matching */
    sno_bind(&s, "alpha=42");
    sno_mark(&s);                           /* mark = "alpha=42" */
    assert(s.mark == s.str.begin);
    assert(sno_span(&s, SNO_LETTERS));      /* match "alpha" */
    assert(sno_cap(&s, buf, sizeof(buf)));
    assert(strcmp(buf, "alpha") == 0);      /* → "alpha" */

    /* Test 3: Mark mid-string, capture remainder */
    sno_bind(&s, "key=value");
    assert(sno_len(&s, 4));                 /* match "key=" */
    sno_mark(&s);                           /* mark = "value" */
    assert(sno_break(&s, "\r\n"));          /* match "value" */
    assert(sno_cap(&s, buf, sizeof(buf)));
    assert(strcmp(buf, "value") == 0);      /* → "value" */

    /* Test 4: Empty capture (mark == cursor) */
    sno_bind(&s, "text");
    sno_mark(&s);                           /* mark = "text" */
    assert(sno_cap(&s, buf, sizeof(buf)));  /* capture zero-length span */
    assert(strcmp(buf, "") == 0);           /* → "" */

    /* Test 5: Buffer overflow fails safely */
    sno_bind(&s, "longtext");
    sno_mark(&s);
    assert(sno_len(&s, 8));                 /* match "longtext" */
    assert(!sno_cap(&s, buf, 5));           /* buf too small (needs 9) */
    assert(s.mark == s.str.begin);          /* mark unchanged after failure */
    assert(s.view.end == s.str.begin + 8);  /* cursor unchanged after failure */

    /* Test 6: sno_reset restores mark to start */
    sno_bind(&s, "resetme");
    assert(sno_len(&s, 3));                 /* match "res" */
    sno_mark(&s);                           /* mark = "etme" */
    sno_reset(&s);                          /* reset cursor AND mark */
    assert(s.mark == s.str.begin);          /* mark back to start */
    assert(s.view.end == s.str.begin);      /* cursor back to start */
    assert(sno_cap(&s, buf, sizeof(buf)));  /* capture empty */
    assert(strcmp(buf, "") == 0);

    /* Test 7: Full identifier capture (real-world usage) */
    sno_bind(&s, "count=42");
    sno_mark(&s);
    assert(sno_any(&s, SNO_LETTERS));
    assert(sno_span(&s, SNO_ALNUM_U));
    assert(sno_cap(&s, buf, sizeof(buf)));
    assert(strcmp(buf, "count") == 0);      /* gives "count" (not "ount") */

    /* sno_notany */
    sno_bind(&s, "42alpha");
    assert(sno_notany(&s, SNO_LETTERS));   // '4' not in letters → success
    assert(s.view.end - s.view.begin == 1);
    assert(sno_notany(&s, SNO_LETTERS));   // '2' not in letters → success
    assert(!sno_notany(&s, SNO_LETTERS));  // 'a' IS in letters → fail
    assert(s.view.end == s.str.begin + 2); // cursor unchanged after failure
    
    sno_bind(&s, "alpha");
    assert(!sno_notany(&s, SNO_LETTERS));  // 'a' in letters → fail immediately
    assert(s.view.begin == s.view.end);    // empty view (cursor at start)
    
    sno_bind(&s, "");
    assert(!sno_notany(&s, SNO_LETTERS));  // empty string → fail
    
    /* NOTANY vs ANY complement test */
    sno_bind(&s, "a1b2");
    assert(sno_any(&s, SNO_LETTERS));      // 'a' in letters
    assert(sno_notany(&s, SNO_LETTERS));   // '1' not in letters
    assert(sno_any(&s, SNO_LETTERS));      // 'b' in letters
    assert(sno_notany(&s, SNO_LETTERS));   // '2' not in letters

    /* sno_tab / sno_rtab / sno_rem */

    /* TAB: absolute positioning forward only */
    sno_bind(&s, "SNOBOL4");
    assert(sno_len(&s, 2));                        // "SN" → cursor at offset 2
    assert(sno_tab(&s, 6));                        // TAB(6): match "OBOL" to offset 6
    assert(s.view.end - s.view.begin == 4);
    assert(memcmp(s.view.begin, "OBOL", 4) == 0);
    assert(s.view.end == s.str.begin + 6);
    
    /* TAB failure: leftward move */
    sno_bind(&s, "text");
    assert(sno_len(&s, 3));                        // cursor at offset 3
    assert(!sno_tab(&s, 2));                       // cannot move left to offset 2
    assert(s.view.end == s.str.begin + 3);         // cursor unchanged
    
    /* RTAB: positioning from right */
    sno_bind(&s, "SNOBOL4");                       // length = 7
    assert(sno_len(&s, 2));                        // "SN" → offset 2
    assert(sno_rtab(&s, 1));                       // RTAB(1): to offset 6 (7-1)
    assert(s.view.end - s.view.begin == 4);
    assert(memcmp(s.view.begin, "OBOL", 4) == 0);
    
    /* REM: match to end */
    sno_bind(&s, "host=alpha");
    assert(sno_len(&s, 5));                        // skip "host="
    assert(sno_rem(&s));                           // match "alpha"
    assert(strcmp(s.view.begin, "alpha") == 0);
    
    /* Zero-length success cases */
    sno_bind(&s, "text");
    assert(sno_tab(&s, 0));                        // TAB(0) at start → empty match
    assert(s.view.begin == s.view.end);
    
    sno_bind(&s, "text");
    assert(sno_len(&s, 4));                        // cursor at end
    assert(sno_rem(&s));                           // REM at end → empty match
    assert(s.view.begin == s.view.end);

    /* sno_at / sno_at_r (position predicates) */

    sno_bind(&s, "0123456789");
    assert(sno_len(&s, 4));                  // cursor at offset 4 ("456789")
    assert(sno_at(&s, 4));                   // at column 4 → true
    assert(!sno_at(&s, 5));                  // not at column 5 → false
    assert(sno_at_r(&s, 6));                 // 6 chars remain → true
    assert(!sno_at_r(&s, 5));                // not 5 chars remaining → false
    
    /* Composition with pattern functions */
    sno_bind(&s, "host=alpha");
    assert(sno_len(&s, 4) && sno_at(&s, 4)); // "host" lands at offset 4
    assert(sno_lit(&s, '=') && sno_at(&s, 5)); // '=' lands at offset 5
    
    /* End-of-string test */
    sno_bind(&s, "text");
    assert(sno_rem(&s) && sno_at_r(&s, 0));  // REM lands at end → 0 chars remaining
    assert(sno_at(&s, 4));                   // length=4 → at offset 4
    
    /* NULL safety */
    assert(!sno_at(NULL, 0));
    assert(!sno_at_r(NULL, 0));

}

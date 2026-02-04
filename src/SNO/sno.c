#include "sno.h"
#include <string.h>

/* Roll back cursor and view to pos on failure; preserves failure contract (cursor unchanged) */
#define sno_rollback(s, pos) ((s) ? ((s)->view.end = (pos), (s)->view.begin = (pos), false) : false)

void sno_bind(sno_subject_t* s, cstr_t* c) {
    if(s && c) {
        s->str.begin = s->str.end = s->view.begin = s->view.end = s->mark = c;
        while(*s->str.end++ != '\0');
        s->length = s->str.end - s->str.begin - 1;
    }
}

bool sno_reset(sno_subject_t* s) {
    if (!s) return false;
    s->view.begin = s->view.end = s->mark = s->str.begin;
    return true;
}

bool sno_lit(sno_subject_t* s, char ch) {
    if(!s) return false;
    cstr_t* pos = s->view.end;
    if(*pos != ch) return false;
    s->view.begin = s->view.end;
    s->view.end = pos + 1;
    return true;
}

bool sno_len(sno_subject_t* s, size_t n) {
    if(!s) return false;
    cstr_t* pos = s->view.end + n;
    if(pos >= s->str.end) return false;  // past end of string
    s->view.begin = s->view.end;
    s->view.end = pos;
    return true;
}

bool sno_span(sno_subject_t* s, const char* set) {
    if(!s || !set) return false;
    cstr_t* start = s->view.end;
    cstr_t* pos = start;
    while(*pos && strchr(set, *pos)) pos++;
    if(pos == start) return false;  // SPAN requires ≥1 char
    s->view.begin = start;
    s->view.end = pos;
    return true;
}

bool sno_break(sno_subject_t* s, const char* set) {
    if(!s || !set) return false;
    cstr_t* start = s->view.end;
    cstr_t* pos = start;
    while(*pos && !strchr(set, *pos)) pos++;
    // BREAK succeeds even with zero-length match
    s->view.begin = start;
    s->view.end = pos;
    return true;
}

bool sno_var(sno_subject_t* s, char* buf, size_t buflen) {
    if(!s || !buf || buflen == 0) return false;
    size_t len = s->view.end - s->view.begin;
    if(len >= buflen) return false;  // not enough space for '\0'
    memcpy(buf, s->view.begin, len);
    buf[len] = '\0';
    return true;
}

bool sno_len_var(sno_subject_t* s, size_t n, char* buf, size_t buflen) {
    if (!s) return false;
    sno_view_t v = s->view;          // save cursor state
    if (!sno_len(s, n)) return false;
    if (sno_var(s, buf, buflen)) return true;
    s->view = v;                     // rollback on extraction failure
    return false;
}

bool sno_mark(sno_subject_t* s) {
    if (!s) return false;
    s->mark = s->view.end;
    return true;
}

bool sno_cap(sno_subject_t* s, char* buf, size_t buflen) {
    if (!s || !buf || !buflen || !s->mark) return false;
    size_t len = s->view.end - s->mark;
    if (len >= buflen) return false;  // no room for null terminator
    memcpy(buf, s->mark, len);
    buf[len] = '\0';
    return true;
}

bool sno_any(sno_subject_t* s, const char* set) {
    if (!s || !set) return false;
    cstr_t* pos = s->view.end;
    if (!*pos || !strchr(set, *pos)) return false;  // not in set or at end
    s->view.begin = pos;
    s->view.end = pos + 1;
    return true;
}

bool sno_notany(sno_subject_t* s, const char* set) {
    if (!s || !set) return false;
    cstr_t* pos = s->view.end;
    if (!*pos || strchr(set, *pos)) return false;  // end of string OR char in set
    s->view.begin = pos;
    s->view.end = pos + 1;
    return true;
}

bool sno_tab(sno_subject_t* s, size_t n) {
    if (!s) return false;
    size_t cur = s->view.end - s->str.begin;
    if (n < cur || n > s->length) return false;  // leftward move or beyond end → fail
    s->view.begin = s->view.end;
    s->view.end = s->str.begin + n;
    return true;
}


bool sno_rtab(sno_subject_t* s, size_t n) {
    if (!s || n > s->length) return false;       // n > length would position before start
    size_t cur = s->view.end - s->str.begin;
    size_t target = s->length - n;
    if (target < cur) return false;              // leftward move → fail
    s->view.begin = s->view.end;
    s->view.end = s->str.begin + target;
    return true;
}

bool sno_rem(sno_subject_t* s) {
    if (!s) return false;
    s->view.begin = s->view.end;
    s->view.end = s->str.end - 1;                // before null terminator
    return true;
}

bool sno_bal(sno_subject_t* s, char open, char close) {
    if (!s) return false;
    
    cstr_t* start = s->view.end;
    char delim[3] = {open, close, '\0'};
    
    if (!sno_lit(s, open)) return false;     /* Match opening delimiter */
    
    while (*s->view.end && *s->view.end != close) {    /* Consume balanced interior */    
        if (*s->view.end == open) {
            if (!sno_bal(s, open, close)) return sno_rollback(s, start);
        } else {
            if (!sno_notany(s, delim)) return sno_rollback(s, start);
        }
    }
    
    if (!sno_lit(s, close)) return sno_rollback(s, start);    /* Match closing delimiter */

    s->view.begin = start; /* Success: view spans entire balanced expression INCLUDING delimiters */
    return true;
}

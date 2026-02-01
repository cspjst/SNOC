#include "sno.h"
#include <string.h>

void sno_bind(sno_subject_t* s, cstr_t* c) {
    if(s && c) {
        s->str.begin = s->str.end = s->view.begin = s->view.end = c;
        while(*s->str.end++ != '\0');
        s->length = s->str.end - s->str.begin - 1;
    }
}

void sno_reset(sno_subject_t* s) {
    if(s) {
        s->view.begin = s->str.begin;
        s->view.end = s->str.begin;  // cursor at start
    }
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

bool sno_ws(sno_subject_t* s) {
   if (!s) return false;
   sno_view_t saved = s->view;  // save current view state
   if (sno_span(s, " \t\r\n")) return true;  // consumed 1+ whitespace → view already set correctly
   // Zero whitespace present → succeed with empty span at current cursor
   s->view.begin = s->view.end;  // [cursor, cursor)
   return true;
}

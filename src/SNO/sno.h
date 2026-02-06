#ifndef SNO_H
#define SNO_H

#include <stddef.h>
#include <stdbool.h>

typedef const char cstr_t;

typedef struct {
    cstr_t* begin;
    cstr_t* end;
} sno_view_t;

typedef struct {
    sno_view_t str;
    sno_view_t view;
    cstr_t* mark;
    size_t length;
} sno_subject_t;

/* === Subject Management === */
void sno_bind(sno_subject_t* s, cstr_t* c);
bool sno_reset(sno_subject_t* s);

/* === Literals === */
bool sno_ch(sno_subject_t* s, char ch);
bool sno_lit(sno_subject_t* s, cstr_t* c);

/* === Length === */
bool sno_len(sno_subject_t* s, size_t n);

/* === Character Sets === */
bool sno_any(sno_subject_t* s, const char* set);
bool sno_notany(sno_subject_t* s, const char* set);
bool sno_span(sno_subject_t* s, const char* set);
bool sno_break(sno_subject_t* s, const char* set);

/* === Positioning === */
bool sno_tab(sno_subject_t* s, size_t n);
bool sno_rtab(sno_subject_t* s, size_t n);
bool sno_rem(sno_subject_t* s);

/* === Capture === */
bool sno_mark(sno_subject_t* s);
bool sno_cap(sno_subject_t* s, char* buf, size_t buflen);
bool sno_var(sno_subject_t* s, char* buf, size_t buflen);

/* === Balanced Delimiters === */
bool sno_bal(sno_subject_t* s, char open, char close);

/* === Position Predicates === */
#define sno_at(s, n) ((s) && (size_t)((s)->view.end - (s)->str.begin) == (n))
#define sno_at_r(s, n) ((s) && (size_t)((s)->view.end - (s)->str.begin) == (s)->length - (n))

#endif

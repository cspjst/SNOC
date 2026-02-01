#ifndef SNO_H
#define SNO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef const char cstr_t;

typedef struct {
    cstr_t* begin;
    cstr_t* end;
} sno_view_t;

typedef struct {
    sno_view_t str;     // right-open interval
    sno_view_t view;    // closed interval
    size_t length;
    bool anchored;
} sno_subject_t;

void sno_reset(sno_subject_t* s);

void sno_anchor(sno_subject_t* s);

void sno_unanchor(sno_subject_t* s);

void sno_var(sno_subject_t* s, char* c);

bool sno_lit(sno_subject_t* s, char ch);

void sno_bind(sno_subject_t* s, cstr_t* c);

bool sno_len(sno_subject_t* s, size_t n);

void sno_fprint(FILE* f, sno_view_t v);

inline void sno_print(sno_view_t v) { sno_fprint(stdout, v); }

void sno_dump(FILE* f, sno_subject_t* s);

#endif

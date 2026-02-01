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
    sno_view_t str;
    cstr_t* cursor;
    cstr_t* needle;
    size_t length;
} sno_subject_t;

void sno_bind(sno_subject_t* s, cstr_t* c);

cstr_t* sno_len(sno_subject_t* s, size_t n);

void sno_fprint(FILE* f, sno_view_t v);

void sno_dump(FILE* f, sno_subject_t* s);

#endif

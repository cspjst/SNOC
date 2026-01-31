#ifndef SNOC_H
#define SNOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef const char cstr_t;
typedef uint32_t snoc_size_t;

typedef struct {
    cstr_t* subject;
    cstr_t* cursor;
    cstr_t* end;
    cstr_t* pre;
    snoc_size_t length;
} snoc_str_t;

typedef struct {
    cstr_t* begin;
    cstr_t* end;
} snoc_view_t;

bool snoc_bind(snoc_str_t* s, cstr_t* c);

snoc_view_t snoc_view(snoc_str_t* s);

bool snoc_write_view(snoc_view_t, FILE* ostream);

inline void snoc_print(snoc_view_t v) { snoc_write_view(v, stdout); }

bool snoc_dump(snoc_str_t* s, FILE* ostream);

#endif

#include "sno.h"

void sno_bind(sno_subject_t* s, cstr_t* c) {
    if(s && c) {
        s->str.begin = s->str.end = s->needle = s->cursor = c;
        while(*s->str.end != '\0') s->str.end++;
        s->length = s->str.end - s->str.begin;
    }
}

void sno_fprint(FILE* f, sno_view_t v) {
    if(f) {
        while(v.begin < v.end) fprintf(f, "%c", *v.begin++);
        fprintf(f, "\n");
    }
}

void sno_dump(FILE* f, sno_subject_t* s) {
    if(s) {
        sno_fprint(f, s->str);
        fprintf(f, "begin %p\n", s->str.begin);
        fprintf(f, "end %p\n", s->str.end);
        fprintf(f, "cursor %p\n", s->cursor);
        fprintf(f, "needle %p\n", s->needle);
        fprintf(f, "length %i\n", s->length);
    }
}

#include "sno.h"
#include <string.h>

void sno_reset(sno_subject_t* s) {
    s->view.begin = s->str.begin;
    s->view.end = s->view.begin;
}

void sno_anchor(sno_subject_t* s) {
    sno_reset(s);
    s->anchored = true;
}

void sno_unanchor(sno_subject_t* s) {
    s->anchored = false;
}

void sno_bind(sno_subject_t* s, cstr_t* c) {
    if(s && c) {
        s->str.begin = s->str.end = s->view.begin = s->view.end = c;
        while(*s->str.end++ != '\0');
        s->length = s->str.end - s->str.begin - 1;
        s->anchored = false;
    }
}

void sno_var(sno_subject_t* s, char* c) {
    if(s) {
        size_t sz = s->view.end - s->view.begin;
        memcpy(c, s->view.begin, sz);
        c[sz] = '\0';
    }
}

bool sno_lit(sno_subject_t* s, char ch) {
    if(!s) return false;
    cstr_t* needle = s->anchored ?s->str.begin :s->view.end;
    if(*needle++ != ch) return false;
    s->view.begin = s->anchored ?s->str.begin :s->view.end;
    s->view.end = needle;
    return true;
}

bool sno_len(sno_subject_t* s, size_t n) {
    if(!s) return false;
    cstr_t* needle = s->anchored ?s->str.begin :s->view.end;
    needle += n;
    if(needle >= s->str.end) return false;
    s->view.begin = s->anchored ?s->str.begin :s->view.end;
    s->view.end = needle;
    return true;
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
        sno_fprint(f, s->view);
        fprintf(f, "str [%p, %p)\n", s->str.begin, s->str.end);
        fprintf(f, "view [%p, %p)\n", s->view.begin, s->view.end);
        fprintf(f, "length %i\n", s->length);
        fprintf(f, "anchored %c\n", s->anchored ?'T' :'F');
    }
}

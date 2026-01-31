#include "snoc.h"

bool snoc_bind(snoc_str_t* s, cstr_t* c) {
    if(s && c) {
        s->subject = s->cursor = s->end = s->pre = c;
        while(*s->end != '\0') s->end++;
        s->length = s->end - s->subject;
        return true;
    }
    else {
        return false;
    }
}

snoc_view_t snoc_view(snoc_str_t* s) {
    snoc_view_t v = {0};
    if(s) {
        v.begin = s->pre;
        v.end = s->cursor;
    }
    return v;
}

bool snoc_write_view(snoc_view_t v, FILE* ostream) {
    if(ostream) {
        while(v.begin < v.end) fprintf(ostream, "%c", v.begin++);
        return true;
    }
    else {
        return false;
    }
}

bool snoc_dump(snoc_str_t* s, FILE* ostream) {
    if(s) {
        if(s->subject) fprintf(ostream, "%s\n", s->subject);
        fprintf(ostream, "subject %p\n", s->subject);
        fprintf(ostream, "cursor %p\n", s->cursor);
        fprintf(ostream, "end %p\n", s->end);
        fprintf(ostream, "pre %p\n", s->pre);
        fprintf(ostream, "length %i\n", s->length);
        return true;
    }
    else {
        return false;
    }
}

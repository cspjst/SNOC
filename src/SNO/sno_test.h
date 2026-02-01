#ifndef SNO_TEST_H
#define SNO_TEST_H

#include "sno.h"
#include <stdio.h>

void sno_test() {
    sno_subject_t s = {0};
    char v[10];
    sno_dump(stdout, &s);
    sno_bind(&s, "(xy)");
    sno_dump(stdout, &s);
    if (sno_lit(&s, '(') && sno_lit(&s, 'x')) sno_print(s.view);
    sno_anchor(&s);
    if (sno_lit(&s, 'x') || sno_lit(&s, '(')) sno_print(s.view);
    if (sno_lit(&s, '(') && sno_lit(&s, 'x')) sno_print(s.view);
    if (sno_lit(&s, 'x') || sno_lit(&s, '(')) sno_print(s.view);
    sno_unanchor(&s);
    if(sno_len(&s, 3)) sno_print(s.view);
    sno_reset(&s);
    if(sno_lit(&s, '(') && sno_len(&s, 3)) sno_print(s.view);
    sno_var(&s, v);
    printf("var %s\n", v);
    sno_reset(&s);
    if(sno_lit(&s, '(') && sno_len(&s, 2) && sno_lit(&s, ')')) sno_print(s.view);
    sno_reset(&s);
    if(sno_lit(&s, '(') && sno_len_var(&s, 2, v) && sno_lit(&s, ']')) printf("var %s\n", v);
    sno_reset(&s);
    if(sno_lit(&s, '(') && sno_len_var(&s, 2, v) && sno_lit(&s, ')')) printf("var %s\n", v);


}

#endif

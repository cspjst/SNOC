#ifndef SNO_TEST_H
#define SNO_TEST_H

#include "sno.h"
#include <stdio.h>

void sno_test() {
    sno_subject_t s = {0};
    sno_dump(stdout, &s);
    sno_bind(&s, "abcd");
    sno_dump(stdout, &s);
}

#endif

#ifndef SNOC_TEST_H
#define SNOC_TEST_H

#include "snoc.h"
#include <stdio.h>

void snoc_test() {
    snoc_str_t s = {0};
    snoc_dump(&s, stdout);
    snoc_print(snoc_view(&s));
    snoc_bind(&s, "abcd");
    snoc_dump(&s, stdout);
    snoc_print(snoc_view(&s));

}

#endif

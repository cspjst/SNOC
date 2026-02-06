#include <stdio.h>
#include "SNO/sno_test.h"
#include "SNO/sno_constants.h"

int main() {
    printf("testing... ");
    sno_test();
    printf("passed!\n");

    sno_subject_t s = {0};
    char key[16], val[16];

    sno_bind(&s, "host=alpha");
    if (sno_span(&s, SNO_ALNUM_U) &&         /* match "host" */
        sno_ch(&s, '=') &&
        sno_mark(&s) &&                      /* mark start of value */
        sno_break(&s, "\r\n") &&             /* match "alpha" */
        sno_cap(&s, val, sizeof(val)) &&     /* capture value */
        sno_reset(&s) &&                     /* reset to start */
        sno_span(&s, SNO_ALNUM_U) &&
        sno_cap(&s, key, sizeof(key)))       /* capture key */
    {
        printf("%s\t%s\n", key, val);
    }
}

#ifdef __ORCAC__
#pragma optimize -1 /* must be at least 8 */
#pragma noroot
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

int asprintf(char **ret, const char *format, ...) {
    va_list va;
    int i;
    int length = -1;
    
    *ret = NULL;
    
    for (i = 0; i < 2; i++) {
        va_start(va, format);
        length = vsnprintf(*ret, length+1, format, va);
        if (length <= 0 || length == INT_MAX) {
            length = -1;
            if (i > 0)
                free(*ret);
            *ret = NULL;
            goto done;
        }
        if (i == 0) {
            *ret = malloc(length+1);
            if (*ret == NULL) {
                length = -1;
                goto done;
            }
#ifndef __ORCAC__
            /* Should be here per standard, but ORCA/C allows only one va_end */
            va_end(va);
#endif
        }

    }

done:
    va_end(va);
    return length;
}

#ifdef TEST_ASPRINTF

int main(void) {
    char *s;
    
    asprintf(&s, "testing %i %i %x", 1, 2, 3);
    printf("%s\n", s);
}

#endif

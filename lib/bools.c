/* ------------------------------------------------------------------------- */
/* bool functions */

#include "bblib.h"

int get_false_true(const char *arg)
{
    if (arg) {
        if (0==stricmp(arg, "true"))
            return 1;
        if (0==stricmp(arg, "false"))
            return 0;
    }
    return -1;
}

const char *false_true_string(int f)
{
    return f ? "true" : "false";
}

void set_bool(void *v, const char *arg)
{
    char *p = (char *)v;
    int f = get_false_true(arg);
    *p = -1 == f ? 0 == *p : 0 != f;
}

/* ------------------------------------------------------------------------- */

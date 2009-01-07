/* ------------------------------------------------------------------------- */
/* number functions */

#include "bblib.h"

int imax(int a, int b)
{
    return a>b?a:b;
}

int imin(int a, int b)
{
    return a<b?a:b;
}

int iminmax(int a, int b, int c)
{
    if (a>c) a=c;
    if (a<b) a=b;
    return a;
}

int iabs(int a)
{
    return a < 0 ? -a : a;
}

int is_alpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

int is_alnum(int c)
{
    return is_alpha(c) || is_digit(c);
}

/* ------------------------------------------------------------------------- */

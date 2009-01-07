/* -------------------------------------------------------------- */
/* install.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#ifdef __BORLANDC__
#include <utime.h>
#else
#include <sys/utime.h>
#endif
#include <sys/stat.h>
#include <direct.h>
#include <errno.h>

#define IS_PATHSEP(c) ((c) == '/' || (c) == '\\')
#define PATHSEP "/"
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* -------------------------------------------------------------- */

char *extract_string(char *d, const char *s, int n)
{
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

char *file_basename(const char *name)
{
    char *p = strchr(name, 0);
    while (p > name && !IS_PATHSEP(p[-1]))
        --p;
    return p;
}

int is_dir(const char *path)
{
    struct stat s;
    int r;
    r = stat(path, &s);
    return r >= 0 && (S_IFDIR & s.st_mode);
}

int is_file(const char *path)
{
    struct stat s;
    int r;
    r = stat(path, &s);
    return r >= 0 && (S_IFREG & s.st_mode);

}

int make_dir(const char *path)
{
    int r = mkdir(path);
    return r >= 0;
}

int set_filetime(const char *path, time_t t)
{
    struct utimbuf ut;
    int r;
    ut.actime = ut.modtime = t;
    r = utime(path, &ut);
    return r >= 0;
}

int get_filetime(const char *path, time_t *t)
{
    struct stat s;
    int r;
    r = stat(path, &s);
    if (r < 0) {
        *t = 0;
        return 0;
    }
    *t = s.st_mtime;
    return 1;
}

/* -------------------------------------------------------------- */

int copy_file(const char *in, const char *out)
{
    FILE *fp, *op;
    char buffer[0x8000];
    size_t len = 1;
    time_t t;

    fp = fopen(in, "rb");
    if (fp) {
        op = fopen(out, "wb");
        if (op) {
            for (;;) {
                len = fread(buffer, 1, sizeof buffer, fp);
                if (0 == len)
                    break;
                if (fwrite(buffer, 1, len, op) != len)
                    break;
            }
            fclose(op);
        }
        fclose(fp);
        if (len) {
            fprintf(stderr, "'%s': %s\n", out, strerror(errno));
            return 0;
        }
        if (get_filetime(in, &t))
            set_filetime(out, t);
        return 1;
    }
    fprintf(stderr, "'%s': %s\n", in, strerror(errno));
    return 0;
}

/* ---------------------------------------------------------------------------- */

int make_folders(const char *p, int all, time_t t)
{
    int m, c;
    char buffer[MAX_PATH];

    for (m = 0; ; ++m) {
        while (0 != (c = p[m]) && !IS_PATHSEP(c))
            ++m;
        if (0 == c && 0 == all)
            break;
        if (0 == m || p[m-1] == ':')
            ++m;
        extract_string(buffer, p, m);

        if (!is_dir(buffer)) {
            if (!make_dir(buffer)) {
                fprintf(stderr, "'%s': %s\n", buffer, strerror(errno));
                return 0;
            }
            if (t)
                set_filetime(buffer, t);
        }
        if (0 == c)
            break;
    }
    return 1;
}

/* -------------------------------------------------------------- */

void join_path(char *buf, const char *dir, const char *file)
{
    char *s, *d, c;
    char tmp[MAX_PATH];

    strcpy(tmp, dir);
    if (*file_basename(dir) && *file)
        strcat(tmp, PATHSEP);
    strcat(tmp, file);

    s = tmp, d = buf;
    do {
        c = *s++, *d++ = IS_PATHSEP(c) ? PATHSEP[0] : c;
    } while (c);
}

/* -------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int ret, i, l, mkdir, quiet, dry, rename, keep;
    char src[MAX_PATH];
    char dst[MAX_PATH];
    const char *dir, *from, *to;
    char *arg, *base, *arg2;
    time_t t_src, t_dst;

    quiet = dry = rename = keep = 0;
    to = from = "";
    dir = NULL;
    ret = 1;

    if (argc < 2)
    {
usage:
        fprintf(stderr,
            """Usage: install DIRECTORY |options] FILES ..."
            "\nInstall FILES into DIRECTORY"
            "\nOptions:"
            "\n -from SUBDIR            read following files from SUBDIR"
            "\n -to SUBDIR              write following files to SUBDIR"
            "\n -subdir SUBDIR          same as -from SUBDIR -to SUBDIR"
            "\n -rename FILE TOFILE     install FILE as TOFILE"
            "\n"
            "\n FILES with trailing slash are created as directories."
            "\n"
            "\n -k(eep)                 install only if destination does not exist" 
            "\n -q(uiet)                be quiet"
            "\n -n(oaction)             dry run, don't install anything"
            "\n --                      reset -from, -to and -keep options."
            "\n"
            "\n");

        goto the_end;
    }

    for (i = 1; i < argc; ++i)
    {
        arg = argv[i];

        if (*arg == '-') {

            if (0 == strcmp(arg, "-to")) {
                if (++i >= argc)
                    goto usage;
                to = argv[i];

            } else if (0 == strcmp(arg, "-from")) {
                if (++i >= argc)
                    goto usage;
                from = argv[i];

            } else if (0 == strcmp(arg, "-subdir")) {
                if (++i >= argc)
                    goto usage;
                from = to = argv[i];

            } else if (0 == strcmp(arg, "-rename")) {
                rename = 1;

            } else {

                l = strlen(arg);
                if (l < 2) {
                    goto usage;

                } else if (0 == memcmp(arg, "-keep", l)) {
                    keep = 1;

                } else if (0 == memcmp(arg, "-quiet", l)) {
                    quiet = 1;

                } else if (0 == memcmp(arg, "-noaction", l)) {
                    dry = 1;

                } else if (0 == memcmp(arg, "--", l)) {
                    from = to = "", keep = 0;

                } else {
                    goto usage;
                }
            }

        } else if (dir == NULL) {

            dir = arg;

        } else {
            if (0 == strcmp(to, "."))
                to = "";
            if (0 == strcmp(from, "."))
                from = "";

            if (rename) {
                if (++i >= argc)
                    goto usage;
                arg2 = argv[i];
                base = file_basename(arg2);
            } else {
                arg2 = file_basename(arg);
                base = arg2;
            }

            if (0 == base[0] || 0 == strcmp(base, ".")) {
                if (rename)
                    goto usage;

                join_path(dst, dir, to);
                join_path(dst, dst, arg);

                base = file_basename(dst);
                if (base > dst)
                    --base;
                *base = 0;

                mkdir = 1;

            } else {
                join_path(src, from, arg);
                join_path(dst, dir, to);
                join_path(dst, dst, arg2);

                mkdir = rename = 0;
            }

            //printf("[%d] %s\n", mkdir, dst);

            get_filetime(dst, &t_dst);
            if (mkdir) {
                if (t_dst)
                    continue;
                if (!quiet)
                    printf("creating directory %s\n", dst);
                if (dry)
                    continue;
                if (!make_folders(dst, 1, 0))
                    goto the_end;

            } else {
                get_filetime(src, &t_src);
                if (t_dst && (keep || t_src <= t_dst))
                    continue;
                if (!quiet)
                    printf("copying %s -> %s\n", src, dst);
                if (dry)
                    continue;
                if (!make_folders(dst, 0, 0))
                    goto the_end;
                if (!copy_file(src, dst))
                    goto the_end;
            }
        }
    }
    ret = 0;

the_end:
    return ret;
}
/* -------------------------------------------------------------- */


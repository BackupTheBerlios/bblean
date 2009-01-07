/*---------------------------------------------------------------------------*

  This file is part of the BBNote source code

  Copyright 2003-2009 grischka@users.sourceforge.net

  BBNote is free software, released under the GNU General Public License
  (GPL version 2). For details see:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.

 *---------------------------------------------------------------------------*/
// bbapi-small.cpp - rc-file reader for blackbox styles

#include "BBApi.h"
#include "bblib.h"
#include "BImage.h"

#define ST static

struct styleprop { const char *key; int  val; };

char pluginrc_path[1];
bool dont_translate_065;

//===========================================================================

void write_rcfile(struct fil_list *fl)
{
    return;
}

struct lin_list *search_line_065(struct fil_list *fl, const char *key)
{
    return NULL;
}

//===========================================================================
// tiny cache reader: first checks, if the file had already been read, if not,
// reads the file into a malloc'ed buffer, then for each line separates
// keyword from value, cuts off leading and trailing spaces, strlwr's the
// keyword and adds both to a list of below defined structures, where k is
// the offset to the start of the value-string. Comments or other non-keyword
// lines have a "" keyword and the line as value.

// added checking for external updates by the user.

// added *wild*card* processing, it first looks for an exact match, if it
// cant find any, returns the first wildcard value, that matches, or null,
// if none...

#define MAX_KEYWORD_LENGTH 100
#define HTS 40 // hash table size

// structures:

struct lin_list
{
    struct lin_list *next;
    struct lin_list *hnext;
    struct lin_list *wnext;
    unsigned hash, k, o;
    int i;
    bool is_wild;
    bool dirty;
    char flags;
    char str[3];
};

struct fil_list
{
    struct fil_list *next;
    struct lin_list *lines;
    struct lin_list *wild;
    struct lin_list *ht[HTS];
    unsigned hash;
    FILETIME ft;
    DWORD tickcount;
    bool dirty;
    bool newfile;
    bool tabify;

    bool is_style;
    bool is_style070;

    int k;
    char path[1];
};

ST struct fil_list *rc_files;
ST struct fil_list *read_file(const char *filename);
ST struct lin_list *make_line (struct fil_list *fl, const char *key, const char *val);
//ST void free_line(struct fil_list *fl, struct lin_list *tl);
ST struct lin_list *search_line(struct fil_list *fl, const char *key, bool fwild, LONG *p_seekpos);

//===========================================================================
// check whether a style uses 0.70 conventions

ST void check_style070(struct fil_list *fl)
{
    struct lin_list *tl;
    dolist (tl, fl->lines)
        if (tl->k > 11 && 0 == memcmp(tl->str+tl->k-11, "appearance", 10))
            break;
    fl->is_style070 = tl || NULL == fl->lines;
}

bool get_070(const char* path)
{
    struct fil_list *fl = read_file(path);
    if (false == fl->is_style) {
        struct fil_list *p;
        dolist (p, rc_files)
            p->is_style = p == fl;
        check_style070(fl);
    }
    return fl->is_style070;
}

//===========================================================================

ST void delete_lin_list(struct fil_list *fl)
{
    freeall(&fl->lines);
    memset(fl->ht, 0, sizeof fl->ht);
    fl->wild = NULL;
}

ST void delete_fil_list(struct fil_list *fl)
{
    if (fl->dirty)
        write_rcfile(fl);
    delete_lin_list(fl);
    remove_item(&rc_files, fl);
}

void write_rcfiles(void)
{
    struct fil_list *fl;
    dolist (fl, rc_files)
        if (fl->dirty)
            write_rcfile(fl);
}

void reset_reader(void)
{
    while (rc_files)
        delete_fil_list(rc_files);
}

//===========================================================================
// helpers

char * read_file_into_buffer (const char *path, int max_len)
{
    FILE *fp; char *buf; int len;
    if (NULL == (fp = fopen(path,"rb")))
        return NULL;

    fseek(fp,0,SEEK_END);
    len = ftell (fp);
    fseek (fp,0,SEEK_SET);
    if (max_len && len >= max_len)
        len = max_len-1;

    buf=(char*)m_alloc(len+1);
    fread (buf, 1, len, fp);
    fclose(fp);

    buf[len]=0;
    return buf;
}

bool is_stylefile(const char *path)
{
    char *temp = read_file_into_buffer(path, 10000);
    bool r = false;
    if (temp) {
        r = NULL != strstr(strlwr(temp), "menu.frame");
        m_free(temp);
    }
    return r;
}

// scan one line in a chat buffer
// advances read pointer, sets start pointer and length
// for the caller, returns first char
char scan_line(char **pp, char **ss, int *ll)
{
    char c, e, *r, *s, *t, *p;
    for (s=r=p=*pp,c=0;;) {
        //skip leading spaces
        for (;0!=(e=*p) && IS_SPC(e) && 10!=e; p++);
        if (r==s) s=r=p, c=e;
        //find end of line
        for (t = r; 0!=(e=*p) && (p++,10!=e); *r = e==9?' ':e, ++r);
        //cut off trailing spaces
        for (; r>t && IS_SPC(r[-1]); r--);
        //check for trailing stray
        if (r == t || r[-1] != '\\')
            break;
        if (r >= t+2 && r[-2] == '\\') { --r; break; }
        // join lines
        for (;--r>s && IS_SPC(r[-1]););
        *r++ = ' ';
    }
    *r=0;
    //ready for next line
    *pp = p, *ss = s, *ll = r-s;
    return c;
}

// strlwr a keyword, calculate hash value and length
unsigned calc_hash(char *p, const char *s, int *pLen, int delim)
{
    unsigned h; int c; char *d = p;
    for (h = 0; 0 != (c = *s) && delim != c; ++s, ++d)
    {
        if (c >= 'A' && c <= 'Z')
            c += 32;
        *d = (char)c;
        if ((h ^= c) & 1)
            h^=0xedb88320;
        h>>=1;
    }
    *d = 0;
    *pLen = d - p;
    return h;
}

//===========================================================================
// XrmResource fake wildcard pattern matcher
// -----------------------------------------
// returns: 0 for no match, else a number that is somehow a measure for
// 'how much' the item matches. Btw 'toolbar*color' means just the
// same as 'toolbar.*.color' and both match e.g. 'toolbar.color'
// as well as 'toolbar.label.color', 'toolbar.button.pressed.color', ...

// this scans one group in a keyword, that is the portion between
// dots, may be literal or '*' or '?'

ST int scan_component(const char **p)
{
    const char *s; char c; int n;
    for (s=*p, n=0; 0 != (c = *s); ++s, ++n) {
        if (c == '*' || c == '?') {
            if (n) break;
            do c = *++s; while (c == '.' || c == '*' || c == '?');
            n = 1;
            break;
        }
        if (c == '.') {
            do c = *++s; while (c == '.');
            break;
        }
    }
    //dbg_printf("scan_component: %d %.*s", n, n, *p);
    *p = s;
    return n;
}

ST int xrm_match (const char *key, const char *pat)
{
    const char *pp, *kk;
    int c, m, n, k, p;
    for (c=256, m=0; ; key=kk, c/=2) {
        kk = key, k = scan_component(&kk);
        pp = pat, p = scan_component(&pp);
        if (0==k) return (0 == p)*m;
        if (0==p) return 0;
        if ('*' == *pat) {
            n = xrm_match(key, pp);
            if (n) return m + n*c/384;
            continue;
        }
        if ('?' != *pat) {
            if (k != p || 0 != memcmp(key, pat, k))
                    return 0; // no match
                m+=c;
            }
            pat=pp;
        }
    }

//===========================================================================

ST struct lin_list *make_line (struct fil_list *fl, const char *key, const char *val)
{
    char buffer[MAX_KEYWORD_LENGTH];
    struct lin_list *tl, **tlp;
    int k, v;
    unsigned h;

    v = strlen(val);
    h = k = 0;
    if (key)
        h = calc_hash(buffer, key, &k, ':');

    tl=(struct lin_list*)c_alloc(sizeof(struct lin_list) + k*2 + v);
    tl->hash = h;
    tl->k = k+1;
    tl->o = k+v+2;
    if (k) {
        memcpy(tl->str, buffer, k);
    memcpy(tl->str + tl->o, key, k);
    }
    memcpy(tl->str+tl->k, val, v);

    //if the key contains a wildcard
    if (k && (memchr(key, '*', k) || memchr(key, '?', k))) {
        // add it to the wildcard - list
        tl->wnext = fl->wild;
        fl->wild = tl;
        tl->is_wild = true;
    } else {
        // link it in the hash bucket
        tlp = &fl->ht[tl->hash%HTS];
        tl->hnext = *tlp;
        *tlp = tl;
    }
    return tl;
}

/*
ST void del_from_list(void *tlp, void *tl, void *n)
{
    void *v; int o = (char*)n - (char*)tl;
    while (NULL != (v = *(void**)tlp)) {
        void **np = (void **)((char *)v+o);
        if (v == tl) { *(void**)tlp = *np; break; }
        tlp = np;
    }
}

ST void free_line(struct fil_list *fl, struct lin_list *tl)
{
    if (tl->is_wild)
        del_from_list(&fl->wild, tl, &tl->wnext);
    else
        del_from_list(&fl->ht[tl->hash%HTS], tl, &tl->hnext);
    m_free(tl);
}
*/

ST struct lin_list *
search_line(struct fil_list *fl, const char *key, bool fwild, LONG *p_seekpos)
{
    int key_len, n;
    char buff[256];
    unsigned h;
    struct lin_list *tl;

    h = calc_hash(buff, key, &key_len, ':');
    if (0 == key_len)
        return NULL;

    ++key_len; // check terminating \0 too

    if (p_seekpos) {
        long seekpos = *p_seekpos;
        n = 0;
        dolist (tl, fl->lines)
            if (++n > seekpos && tl->hash == h && 0==memcmp(tl->str, buff, key_len)) {
                *p_seekpos = n;
                break;
        }
        return tl;
    }

    // search hashbucket
    for (tl = fl->ht[h % HTS]; tl; tl = tl->hnext)
        if (0==memcmp(tl->str, buff, key_len))
            return tl;

    if (fwild) {
        // search wildcards
        struct lin_list *wl;
        int best_match = 0;

        for (wl = fl->wild; wl; wl = wl->wnext) {
            n = xrm_match(buff, wl->str);
            //dbg_printf("match:%d <%s> <%s>", n, buff, sl->str);
            if (n > best_match)
                tl = wl, best_match = n;
    }
    }
    return tl;
}

//===========================================================================
// searches for the filename and, if not found, builds a _new line-list

ST struct fil_list *read_file(const char *filename)
{
    struct lin_list **slp, *sl;
    struct fil_list **flp, *fl;
    char *buf, *p, *d, *s, *t, c; int k;

    DWORD ticknow = GetTickCount();
    char hashname[MAX_PATH];
    unsigned h = calc_hash(hashname, filename, &k, 0);
    k = k + 1;

    // ----------------------------------------------
    // first check, if the file has already been read
    for (flp = &rc_files; NULL!=(fl=*flp); flp = &fl->next) {
        if (fl->hash==h && 0==memcmp(hashname, fl->path+fl->k, k)) {
            // re-check the timestamp after 20 ms
            if (fl->tickcount + 20 < ticknow) {
                //dbg_printf("check time: %s", path);
                if (false == fl->dirty
                    && diff_filetime(fl->path, &fl->ft)) {
                    // file was externally updated
                    delete_lin_list(fl);
                    remove_node(&rc_files, fl);
                    goto addfile;
                    //delete_fil_list(fl);
                    //break;
                }
                fl->tickcount = ticknow;
            }
            return fl; //... return cached line list.
        }
    }

    // ----------------------------------------------
    // limit cached files
    fl = (struct fil_list *)nth_node(rc_files, 4);
    while (fl) {
        struct fil_list *n = fl->next;
        if (false == fl->is_style)
            delete_fil_list(fl);
        fl = n;
    }

    // allocate a _new file structure, the filename is
    // stored twice, as is and strlwr'd for compare.
    fl = (struct fil_list*)c_alloc(sizeof(*fl) + k*2);
    memcpy(fl->path, filename, k);
    memcpy(fl->path+k, hashname, k);
    fl->k = k;
    fl->hash = h;

addfile:
    cons_node(&rc_files, fl);
    fl->tickcount = ticknow;

    //dbg_printf("reading file %s", filename);
    buf = read_file_into_buffer(fl->path, 0);
    if (NULL == buf) {
        fl->newfile = true;
        return fl;
    }

    //set timestamp
    get_filetime(fl->path, &fl->ft);

    for (slp = &fl->lines, p = buf;;) {
        c = scan_line(&p, &s, &k);
        if (0 == c)
            break;
        if (0 == k || c == '#' || c == '!' || NULL == (d = (char*)memchr(s, ':', k))) {
            // this is an empty line or comment or not a keyword
            sl = make_line(fl, NULL, s);
        } else {
            for (t = d; t > s && IS_SPC(t[-1]); --t);
            *t = 0;
            if (t-s > MAX_KEYWORD_LENGTH-1)
                continue;
            //skip spaces between key and value
            while (*++d == ' ');
            //put it into the line structure
            sl = make_line(fl, s, d);
        }
        //append it to the list
        slp = &(*slp=sl)->next;
    }

    if (fl->is_style)
        check_style070(fl);

    m_free(buf);
    return fl;
}

//===========================================================================
// API: ReadValue
// Purpose: Searches the given file for the supplied keyword and returns a
// pointer to the value - string
// In: LPCSTR path = String containing the name of the file to be opened
// In: LPCSTR szKey = String containing the keyword to be looked for
// In: LONG ptr: optional: an index into the file to start search.
// Out: LPSTR = Pointer to the value string of the keyword

LPCSTR ReadValue(LPCSTR path, LPCSTR szKey, LONG *ptr)
{
    // xoblite-flavour plugins bad version test workaround
    struct fil_list *fl = read_file(path[0]?path:stylePath(NULL));
    struct lin_list *tl;
    const char *r = NULL;

    tl = search_line(fl, szKey, true, ptr);
    if (NULL == tl && fl->is_style && false == dont_translate_065)
        tl = search_line_065(fl, szKey);
    if (tl)
        r = tl->str + tl->k;

    //static int rcc; dbg_printf("read %d %s:%s <%s>", ++rcc, path, szKey, r);
    return r;
}


//===========================================================================

//===========================================================================

// API: ReadBool
bool ReadBool(LPCSTR fileName, LPCSTR szKey, bool bDefault)
{
    LPCSTR szValue = ReadValue(fileName, szKey, NULL);
    if (szValue) {
        if (!stricmp(szValue, "true"))
            return true;
        if (!stricmp(szValue, "false"))
            return false;
    }
    return bDefault;
}

// API: ReadInt
int ReadInt(LPCSTR fileName, LPCSTR szKey, int nDefault)
{
    LPCSTR szValue = ReadValue(fileName, szKey, NULL);
    return szValue ? atoi(szValue) : nDefault;
}

// API: ReadString
LPCSTR ReadString(LPCSTR fileName, LPCSTR szKey, LPCSTR szDefault)
{
    LPCSTR szValue = ReadValue(fileName, szKey, NULL);
    return szValue ? szValue : szDefault;
}

// API: ReadColor
COLORREF ReadColor(LPCSTR fileName, LPCSTR szKey, LPCSTR defaultColor)
{
    LPCSTR szValue = szKey[0] ? ReadValue(fileName, szKey, NULL) : NULL;
    return ReadColorFromString(szValue ? szValue : defaultColor);
}

//===========================================================================
// API: ParseItem
// Purpose: parses a given string and assigns settings to a StyleItem class

const struct styleprop styleprop_1[] = {
 {"solid"        ,B_SOLID           },
 {"horizontal"   ,B_HORIZONTAL      },
 {"vertical"     ,B_VERTICAL        },
 {"crossdiagonal",B_CROSSDIAGONAL   },
 {"diagonal"     ,B_DIAGONAL        },
 {"pipecross"    ,B_PIPECROSS       },
 {"elliptic"     ,B_ELLIPTIC        },
 {"rectangle"    ,B_RECTANGLE       },
 {"pyramid"      ,B_PYRAMID         },
 {NULL           ,-1                }
 };
const struct styleprop styleprop_2[] = {
 {"flat"        ,BEVEL_FLAT     },
 {"raised"      ,BEVEL_RAISED   },
 {"sunken"      ,BEVEL_SUNKEN   },
 {NULL          ,-1             }
 };
const struct styleprop styleprop_3[] = {
 {"bevel1"      ,BEVEL1 },
 {"bevel2"      ,BEVEL2 },
 {"bevel3"      ,BEVEL2+1 },
 {NULL          ,-1     }
 };

int findtex(const char *p, const struct styleprop *s)
{
    do if (strstr(p, s->key)) break;
    while ((++s)->key);
    return s->val;
}

void ParseItem(LPCSTR szItem, StyleItem *item)
{
    char buf[256]; int t;
    strlwr(strcpy(buf, szItem));
    t = item->parentRelative = NULL != strstr(buf, "parentrelative");
    if (t) {
        item->type = item->bevelstyle = item->bevelposition = item->interlaced = 0;
        return;
    }
    t = findtex(buf, styleprop_1);
    item->type = (-1 != t) ? t : strstr(buf, "gradient") ? B_DIAGONAL : B_SOLID;

    t = findtex(buf, styleprop_2);
    item->bevelstyle = (-1 != t) ? t : BEVEL_RAISED;

    t = BEVEL_FLAT == item->bevelstyle ? 0 : findtex(buf, styleprop_3);
    item->bevelposition = (-1 != t) ? t : BEVEL1;

    item->interlaced = NULL!=strstr(buf, "interlaced");
}   

//===========================================================================

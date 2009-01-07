/*
 ============================================================================

  This file is part of the bbStyleMaker source code
  Copyright 2003-2009 grischka@users.sourceforge.net

  http://bb4win.sourceforge.net/bblean
  http://developer.berlios.de/projects/bblean

  bbStyleMaker is free software, released under the GNU General Public
  License (GPL version 2). For details see:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.

 ============================================================================
*/

//===========================================================================
void init_root(struct rootinfo *r)
{
    // clear and set some default values
    memset(r, 0, sizeof *r);
    r->modx = r->mody = 4;
    r->sat = 255;
    r->scale = 100;
    r->type = B_SOLID;
}

static const char *switches[] =
{
    "-solid",       "-gradient",    "-mod",
    "-from",        "-bg",          "-background",
    "-to",          "-fg",          "-foreground",
    "interlaced",

    "-tile",        "-center",      "-full",
    "-t",           "-c",           "-f",

    "-bitmap",      "-hue",         "-sat",
    "tile",         "center",       "stretch",

    "-scale",

    NULL
};

enum
{
    E_eos = 0   , E_other,

    E_solid     , E_gradient  , E_mod       ,
    E_from      , E_bg        , E_background ,
    E_to        , E_fg        , E_foreground ,
    Einterlaced ,

    E_tile      , E_center    , E_full      ,
    E_t         , E_c         , E_f         ,

    E_bitmap    , E_hue       , E_sat       ,
    Etile       , Ecenter     , Estretch    ,

    E_scale

};

int next_token(struct rootinfo *r)
{
    strlwr(NextToken(r->token, &r->cptr, NULL));
    if (r->token[0])
        return get_string_index(r->token, switches) + E_other + 1;
    return E_eos;
}

bool read_color(const char *token, COLORREF *pCR)
{
    COLORREF CR = ReadColorFromString(token);
    if ((COLORREF)-1 == CR) return false;
    *pCR = CR;
    return true;
}

bool read_int(const char *token, int *ip)
{
    const char *p = token;
    if ('-' == *p) ++p;
    if (*p < '0' || *p > '9') return false;
    *ip = atoi(token);
    return true;
}

bool parse_command(struct rootinfo *r, const char *command)
{
    r->cptr = command;
    for (;;)
    {
        int s = next_token(r);
cont_1:
        switch (s)
        {
        case E_eos:
            return true;

        default:
            if ('-' == r->token[0])
                return false;
            goto get_img_1;

        case E_tile:
        case E_t:
            goto img_tile;
        case E_full:
        case E_f:
            goto img_full;
        case E_center:
        case E_c:
            goto img_center;

        case E_bitmap:
            s = next_token(r);
            switch (s) {
            case Etile:
            img_tile:
                r->wpstyle = WP_TILE;
                break;
            case Ecenter:
            img_center:
                r->wpstyle = WP_CENTER;
                break;
            case Estretch:
            img_full:
                r->wpstyle = WP_FULL;
                break;
            default:
                goto get_img_1;
            }

            s = next_token(r);
        get_img_1:
            if (E_eos == s) return false;
            if (r->bmp) return false;
            unquote(strcpy(r->wpfile, r->token));
            r->bmp = 1;
            continue;

        case E_solid:
            r->solid = 1;
            s = next_token(r);
            if (s == Einterlaced)
            {
                r->interlaced = true;
                s = next_token(r);
                if (s != E_other)
                    goto cont_1;
            }
            if (false==read_color(r->token, &r->color1))
                return false;
            if (r->interlaced)
                r->color2 = shadecolor(r->color1, -40);
            continue;

        case E_bg:
        case E_background:
        case E_from:
            next_token(r);
            if (false==read_color(r->token, &r->color1))
                return false;
            continue;

        case E_to:
            next_token(r);
            if (false==read_color(r->token, &r->color2))
                return false;
            continue;

        case E_fg:
        case E_foreground:
            next_token(r);
            if (false==read_color(r->token, &r->modfg))
                return false;
            if (r->solid && r->interlaced)
                r->color2 = r->modfg;
            continue;

        case E_gradient:
            r->gradient = 1;
            r->type = B_HORIZONTAL;
            for (;;) {
                int n, f = 0;
                s = next_token(r);
                if (E_eos == s || '-' == r->token[0])
                    break;

                n = findtex(r->token, styleprop_1);
                if (-1 != n) r->type = n; else ++f;

                n = findtex(r->token, styleprop_2);
                if (-1 != n) r->bevelstyle = n; else ++f;

                n = findtex(r->token, styleprop_3);
                if (-1 != n) r->bevelposition = n; else ++f;

                n = NULL != strstr(r->token, "interlaced");
                if (0 != n) r->interlaced = true; else ++f;

                if (f==4)
                    break;
            }
            if (r->bevelstyle) {
                if (0 == r->bevelposition)
                    r->bevelposition = BEVEL1;
            } else {
                if (0 != r->bevelposition)
                    r->bevelstyle = BEVEL_RAISED;
            }
            goto cont_1;

        case E_mod:
            r->mod = 1;
            next_token(r);
            if (false == read_int(r->token, &r->modx))
                return false;
            next_token(r);
            if (false == read_int(r->token, &r->mody))
                return false;
            continue;

        case Einterlaced:
            r->interlaced = true;
            continue;

        case E_hue:
            next_token(r);
            if (!read_int(r->token, &r->hue))
                return false;
            continue;

        case E_sat:
            next_token(r);
            if (!read_int(r->token, &r->sat))
                return false;
            continue;

        case E_scale:
            next_token(r);
            if (NULL == strchr(r->token, '%'))
                return false;
            if (!read_int(r->token, &r->scale))
                return false;
            continue;
        }
    }
}

//===========================================================================

//===========================================================================
int bsetroot_parse(NStyleStruct *pss, const char *command)
{
    char token[MAX_PATH];
    const char *cptr = command;

    struct rootinfo *r = &pss->rootInfo;
    struct NStyleItem *pSI = &pss->rootStyle;
    int f;

    memset(r, 0, sizeof *r);
    memset(pSI, 0, sizeof *pSI);

    strlwr(NextToken(token, &cptr, NULL));
    //f = strstr(token, "bsetroot") || strstr(token, "bsetbg");

    init_root(r);
    f = parse_command(r, cptr) && (r->gradient || r->solid || r->mod);

    pSI->nVersion       = STYLEITEM_VERSION;
    pSI->type           = r->type;
    pSI->Color          = r->color1;
    pSI->ColorTo        = r->color2;
    pSI->interlaced     = r->interlaced;
    pSI->bevelstyle     = r->bevelstyle;
    pSI->bevelposition  = r->bevelposition;
    pSI->TextColor      = r->modfg;

    if (0 == f)
        pSI->parentRelative = true;

    return 1;
}

void make_bsetroot_string(NStyleStruct *pss, char *out, int all)
{
    COLORREF c1, c2;
    int t, x, i, bp, bs;
    char b1[40], b2[40];
    extern int style_version;

    struct rootinfo *r = &pss->rootInfo;
    struct NStyleItem *pSI = &pss->rootStyle;

    memset(out, 0, sizeof pss->rootCommand);

    r->type           = pSI->type           ;
    r->color1         = pSI->Color          ;
    r->color2         = pSI->ColorTo        ;
    r->interlaced     = pSI->interlaced     ;
    r->bevelstyle     = pSI->bevelstyle     ;
    r->bevelposition  = pSI->bevelposition  ;
    r->modfg          = pSI->TextColor      ;
    r->bmp            = 0 != r->wpfile[0];

    c1 = r->color1;
    c2 = r->color2;
    t =  r->type;
    i =  r->interlaced;
    bp = r->bevelposition;
    bs = r->bevelstyle;
    if (pSI->parentRelative)
        t = -1;

    x = 0;

    if (t == -1 && (0 == all || 0 == r->bmp)) {
        out[x] = 0;
        if (all) return;
    }

    x += sprintf(out+x, "bsetroot");
    if (B_SOLID == t && (false == i || write_070)) {
        if (r->mod) {
            x += sprintf(out+x, " -mod %d %d -bg %s -fg %s",
                r->modx,
                r->mody,
                rgb_string(b1, c1),
                rgb_string(b2, pSI->TextColor)
                );
        } else if (i) {
            if (style_version < 4)
                x += sprintf(out+x, " -solid interlaced %s",
                    rgb_string(b1, c1)
                    );
            else
                x += sprintf(out+x, " -solid interlaced -bg %s -fg %s",
                    rgb_string(b1, c1),
                    rgb_string(b2, c2)
                    );
        } else {
            x += sprintf(out+x, " -solid %s", rgb_string(b1, c1));
        }

    } else if (t >= 0) {

        if (B_SOLID == t)
            c2 = c1, t = B_HORIZONTAL;

        x += sprintf(out+x, " -gradient %s%s%s%sgradient -from %s -to %s",
            i ? "interlaced":"",
            bs ? styleprop_2[bs].key : "",
            bs && bp > BEVEL1 ? "bevel2" : "",
            styleprop_1[1+styleprop_1[1+t].val].key,
            rgb_string(b1, c1),
            rgb_string(b2, c2)
            );

        if (r->mod)
            x += sprintf(out+x, " -mod %d %d -fg %s",
                r->modx,
                r->mody,
                rgb_string(b1, pSI->TextColor)
                );
    }

    if (all && r->bmp) {
        t = r->wpstyle;

        if (WP_NONE != t)
            x += sprintf(out+x, " %s", switches[t + (E_tile - 2 - 1)]);

        if (strchr(r->wpfile, ' '))
            x += sprintf(out+x, " \"%s\"", r->wpfile);
        else
            x += sprintf(out+x, " %s", r->wpfile);

        if (r->scale && r->scale != 100)
            x += sprintf(out+x, " -scale %d%%", r->scale);
        if (r->sat < 255)
            x += sprintf(out+x, " -sat %d", r->sat);
        if (r->hue > 0)
            sprintf(out+x, " -hue %d", r->hue);
    }

    //dbg_printf("%s", out);
}

//===========================================================================

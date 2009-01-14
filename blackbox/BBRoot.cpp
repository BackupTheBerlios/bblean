/* ==========================================================================

  This file is part of the bbLean source code
  Copyright © 2001-2003 The Blackbox for Windows Development Team
  Copyright © 2004-2009 grischka

  http://bb4win.sourceforge.net/bblean
  http://developer.berlios.de/projects/bblean

  bbLean is free software, released under the GNU General Public License
  (GPL version 2). For details see:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.

  ========================================================================== */

// execute a bsetroot command and load the generated <tempfile.bmp>
// file into a HBITMAP, for Desk.cpp to paint the wallpaper.

#include "BB.h"
#include "bbrc.h"
#include "Settings.h"
#define ST static

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifndef BBTINY

struct rootinfo
{
    char mod;       // -mod
    char solid;     // -solid
    char gradient;  // -gradient

    COLORREF color1;
    COLORREF color2;
    bool interlaced;
    int type;
    int bevelstyle;
    int bevelposition;

    int modx;
    int mody;
    COLORREF modfg;

    const char *cptr;
    char token[MAX_PATH];
};

void init_root(struct rootinfo *r)
{
    // clear and set some default values
    memset(r, 0, sizeof *r);
    r->modx = r->mody = 4;
    r->type = B_SOLID;
}

static const char * const switches[] = {
    "-solid",       "-gradient",    "-mod",
    "-from",        "-bg",          "-background",
    "-to",          "-fg",          "-foreground",
    "interlaced",
    NULL
};

enum rootcommand_tokens
{
    E_eos = 0   , E_other,

    E_solid     , E_gradient  , E_mod       ,
    E_from      , E_bg        , E_background ,
    E_to        , E_fg        , E_foreground ,
    Einterlaced
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
    if (CLR_INVALID == CR) return false;
    *pCR = CR;
    return true;
}

bool read_int(const char *token, int *ip)
{
    const char *p = token;
    if ('-' == *p) ++p;
    if (*p < '0' || *p > '9')
        return false;
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
            return false;

        case E_solid:
            r->solid = 1;
            if (next_token(r) == Einterlaced)
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
            if (false==read_color(r->token, &r->color1)) return false;
            continue;

        case E_to:
            next_token(r);
            if (false==read_color(r->token, &r->color2)) return false;
            continue;

        case E_fg:
        case E_foreground:
            next_token(r);
            if (false==read_color(r->token, &r->modfg)) return false;
            if (r->solid && r->interlaced)
                r->color2 = r->modfg;
            continue;

        case E_gradient:
            r->gradient = 1;
            r->type = B_HORIZONTAL;
            for (;;) {
                int n, f = 0;
                s = next_token(r);
                if (E_eos == s || '-' == r->token[0]) break;

                n = findtex(r->token, 1);
                if (-1 != n) r->type = n; else ++f;

                n = findtex(r->token, 2);
                if (-1 != n) r->bevelstyle = n; else ++f;

                n = findtex(r->token, 3);
                if (-1 != n) r->bevelposition = n; else ++f;

                n = NULL != strstr(r->token, "interlaced");
                if (0 != n) r->interlaced = true; else ++f;

                if (f==4) break;
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
            if (false == read_int(r->token, &r->modx)) return false;
            next_token(r);
            if (false == read_int(r->token, &r->mody)) return false;
            continue;

        case Einterlaced:
            r->interlaced = true;
            continue;
        }
    }
}

//===========================================================================
void Modula(HDC hdc, int width, int height, int mx, int my, COLORREF fg)
{
    int x, y;
    HGDIOBJ P0 = SelectObject(hdc, CreatePen(PS_SOLID, 1, fg));
    if (my > 1) for (y = height-my; y >= 0; y-=my)
        MoveToEx(hdc, 0, y, NULL), LineTo(hdc, width, y);
    if (mx > 1) for (x = mx-1; x < width;  x+=mx)
        MoveToEx(hdc, x, 0, NULL), LineTo(hdc, x, height);
    DeleteObject(SelectObject(hdc, P0));
}

//===========================================================================
HBITMAP make_root_bmp(const char *command)
{
    struct rootinfo RI;
    struct rootinfo *r = &RI;
    HBITMAP bmp = NULL;

    init_root(r);
    if (parse_command(r, command) && (r->gradient + r->solid + r->mod))
    {
        int width, height;
        RECT rect;
        HWND hwnd_desk;
        HDC hdc_desk, buf;
        HGDIOBJ B0;

        width = VScreenWidth;
        height = VScreenHeight;
        rect.left = rect.top = 0, rect.right = width, rect.bottom = height;

        hwnd_desk = GetDesktopWindow();
        hdc_desk = GetDC(hwnd_desk);
        buf = CreateCompatibleDC(hdc_desk);
        bmp = CreateCompatibleBitmap(hdc_desk, width, height);
        B0 = SelectObject(buf, bmp);

        MakeGradient(buf, rect,
            r->type, r->color1, r->color2, r->interlaced,
            r->bevelstyle, r->bevelposition,
            0, 0, 0
            );

        if (r->mod)
            Modula (buf, width, height, r->modx, r->mody, r->modfg);

        SelectObject(buf, B0);
        DeleteDC(buf);

        ReleaseDC(hwnd_desk, hdc_desk);
    }
    return bmp;
}

//===========================================================================

//===========================================================================
static HBITMAP read_bitmap(const char* path, bool delete_after)
{
    HWND hwnd_desk = GetDesktopWindow();
    HDC hdc_desk = GetDC(hwnd_desk);
    BITMAP bm;
#if 0
    HBITMAP bmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0,0, LR_LOADFROMFILE);
#else
    HBITMAP bmp = NULL;
    FILE *fp=fopen(path, "rb");
    if (fp)
    {
        BITMAPFILEHEADER hdr;
        fread(&hdr, 1, sizeof(hdr), fp);
        if (0x4D42 == hdr.bfType)
        {
            BITMAPINFOHEADER bih, *pbih; int CU, s; void *lpBits;
            fread(&bih, 1, sizeof(bih), fp);
            CU = bih.biClrUsed * sizeof(RGBQUAD);
            pbih = (PBITMAPINFOHEADER)m_alloc(bih.biSize + CU);
            memmove(pbih, &bih, bih.biSize);
            fread(&((BITMAPINFO*)pbih)->bmiColors, 1, CU, fp);
            s = hdr.bfSize - hdr.bfOffBits;
            lpBits = m_alloc(s);
            fseek(fp, hdr.bfOffBits, SEEK_SET);
            fread(lpBits, 1, s, fp);
            bmp = CreateDIBitmap(hdc_desk, pbih, CBM_INIT, lpBits, (LPBITMAPINFO)pbih, DIB_RGB_COLORS);
            m_free(lpBits);
            m_free(pbih);
        }
        fclose(fp);
    }
#endif
    if (bmp && GetObject(bmp, sizeof bm, &bm))
    {
        // convert in any case (20ms), bc if it's compatible, it's faster to paint.
        HDC hdc_old = CreateCompatibleDC(hdc_desk);
        HGDIOBJ old_bmp = SelectObject(hdc_old, bmp);
        HDC hdc_new = CreateCompatibleDC(hdc_desk);
        HBITMAP bmp_new = CreateCompatibleBitmap(hdc_desk, VScreenWidth, VScreenHeight);
        SelectObject(hdc_new, bmp_new);
        StretchBlt(hdc_new, 0, 0, VScreenWidth, VScreenHeight, hdc_old, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        DeleteDC(hdc_new);
        DeleteObject(SelectObject(hdc_old, old_bmp));
        DeleteDC(hdc_old);
        bmp = bmp_new;
    }

    ReleaseDC(hwnd_desk, hdc_desk);
    if (delete_after)
        DeleteFile(path);
    return bmp;
}

#endif //ndef BBTINY

//===========================================================================

//===========================================================================
ST bool is_bsetroot_command(const char **cptr)
{
    char token[MAX_PATH];
    *(char*)file_extension(NextToken(token, cptr, NULL)) = 0;
    return 0 == stricmp(token, "bsetroot") || 0 == stricmp(token, "bsetbg");
}

//===========================================================================
HBITMAP load_desk_bitmap(const char* command, bool makebmp)
{
    char exe_path[MAX_PATH];
    char bmp_path[MAX_PATH];
    char buffer[4*MAX_PATH];
    int x, r;

    const char *cptr;
    unsigned long bsrt_vernum;

    cptr = command;
    if (false == is_bsetroot_command(&cptr)) {
bbexec:
        BBExecute_string(command, RUN_NOERRORS);
        return NULL;
    }

#ifndef BBTINY
    if (makebmp) {
        HBITMAP bmp = make_root_bmp(cptr);
        if (bmp)
            return bmp;
    }
#endif

    set_my_path(NULL, exe_path, "bsetroot.exe");
    bsrt_vernum = getfileversion(exe_path);
    //dbg_printf("bsrt_vernum %08x", bsrt_vernum);
    if (bsrt_vernum < 0x02000000)
        goto bbexec;

    x = sprintf(buffer, "\"%s\" %s", exe_path, cptr);

#ifndef BBTINY
    if (makebmp) {
        set_my_path(NULL, bmp_path, "$bsroot$.bmp");
        x += sprintf(buffer + x, " -save \"%s\"", bmp_path);
    }
#endif

#if 0
    if (bsrt_vernum >= 0x02010000) {
        char base_path[MAX_PATH];
        file_directory(base_path, bbrcPath(NULL));
        x += sprintf(buffer + x, " -prefix \"%s\"", base_path);
    }
#endif

    r = BBExecute_string(buffer, RUN_NOSUBST|RUN_WAIT|RUN_NOERRORS);
    //dbg_printf("command <%s>", buffer);
    if (0 == r)
        return NULL;

#ifndef BBTINY
    if (makebmp)
        return read_bitmap(bmp_path, true);
#endif

    return NULL;
}

//===========================================================================

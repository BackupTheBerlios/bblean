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

// BBSendData.h

// This file implements a common way to send and receive messages and data
// across processes.

// Its used to send broams, style properties, register messages, etc.

// This file is included by blackbox.exe as well as in other applications
// like bbStyleMaker, bbNote, ...

//=====================================================
struct bb_senddata
{
    WPARAM wParam;
#ifndef _WIN64
    DWORD fill;
#endif
    BYTE lParam_data[2000];
};

#ifndef offsetof
# define offsetof(s,m) ((size_t)&(((s*)0)->m))
#endif

//=====================================================
// Send a message with data to bblean from another process
// returns: nonzero on success, zero on error

// Usage example: Set a style
// BBSendData(FindWindow("BlackboxClass", "Blackbox"), BB_SETSTYLE, stylefile, 1+strlen(stylefile));

BOOL BBSendData(
    HWND BBhwnd,        // blackbox core window
    UINT msg,           // the message to send
    WPARAM wParam,      // wParam
    LPCVOID lParam,     // lParam
    int lParam_size     // size of the data referenced by lParam
    )
{
    struct bb_senddata BBSD, *pBBSD;
    unsigned s;
    COPYDATASTRUCT cds;
    BOOL result = FALSE;

    if (NULL == BBhwnd)
        return result;

    if (-1 == lParam_size)
        lParam_size = 1+strlen((const char*)lParam);

    s = lParam_size + offsetof(bb_senddata, lParam_data);

    // for speed, the local buffer is used when sufficient
    if (s <= sizeof BBSD)
        pBBSD = &BBSD;
    else
        pBBSD = (struct bb_senddata*)malloc(s);

    pBBSD->wParam = wParam;
#ifndef _WIN64
    pBBSD->fill = 0;
#endif
    memcpy(pBBSD->lParam_data, lParam, lParam_size);

    cds.dwData = msg;
    cds.cbData = s;
    cds.lpData = (void*)pBBSD;
    result = (BOOL)SendMessage (BBhwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    if (pBBSD != &BBSD)
        free(pBBSD);
    return result;
}

//=====================================================
UINT BBReceiveData(HWND hwnd, LPARAM lParam, int (*fn) (
        HWND hwnd, UINT msg, WPARAM wParam, const char *data, unsigned data_size
        ))
{
    struct bb_senddata *pBBSD;
    UINT msg = (UINT)((COPYDATASTRUCT*)lParam)->dwData;
    if (msg < BB_MSGFIRST || msg >= BB_MSGLAST)
        return TRUE;

    pBBSD = (struct bb_senddata*)((COPYDATASTRUCT*)lParam)->lpData;
    if (fn && fn(hwnd, msg, pBBSD->wParam,
        (const char *)pBBSD->lParam_data,
        ((PCOPYDATASTRUCT)lParam)->cbData - + offsetof(bb_senddata, lParam_data)
        )) return TRUE;

    if (BB_SENDDATA == msg)
        memcpy((void*)pBBSD->wParam, pBBSD->lParam_data, ((PCOPYDATASTRUCT)lParam)->cbData);
    else
        SendMessage(hwnd, msg, pBBSD->wParam, (LPARAM)pBBSD->lParam_data);

    return TRUE;
}

//=====================================================
